#include "GStreamerCamera.h"
#include "aegis/platform/CudaAllocator.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace aegis::core::drivers {

    GStreamerCamera::GStreamerCamera(const std::string& pipeline_str)
        : pipeline_str_(pipeline_str) {
        gst_init(nullptr, nullptr);
    }

    GStreamerCamera::~GStreamerCamera() {
        is_running_ = false;
        if (pipeline_) {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
            gst_object_unref(pipeline_);
        }
        if (pinned_buffer_) {
            platform::CudaAllocator::free_pinned(pinned_buffer_);
        }
    }

    bool GStreamerCamera::initialize() {
        // --- 1. Construct the GStreamer Pipeline ---
        // We append the 'appsink' to the user's string to capture the output.
        // The videoconvert ensures we get a predictable RGB format.
        std::string full_pipeline = pipeline_str_ +
            " ! videoconvert ! video/x-raw,format=RGB ! appsink name=aegis_sink emit-signals=true drop=true max-buffers=2";

        GError *error = nullptr;
        pipeline_ = gst_parse_launch(full_pipeline.c_str(), &error);

        if (error || !pipeline_) {
            spdlog::critical("[GSCam] Pipeline Parse Error: {}", error ? error->message : "Unknown");
            return false;
        }

        // --- 2. Find the AppSink and Connect Callbacks ---
        appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "aegis_sink");
        if (!appsink_) {
            spdlog::critical("[GSCam] Could not find 'aegis_sink' in pipeline!");
            return false;
        }
        g_signal_connect(appsink_, "new-sample", G_CALLBACK(on_new_sample), this);
        gst_object_unref(appsink_); // gst_bin_get_by_name adds a reference

        // --- 3. Setup Bus Watch for Errors ---
        GstBus *bus = gst_element_get_bus(pipeline_);
        gst_bus_add_watch(bus, (GstBusFunc)on_bus_message, this);
        gst_object_unref(bus);

        // --- 4. Start the Pipeline ---
        GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            spdlog::critical("[GSCam] Unable to set pipeline to PLAYING state.");
            return false;
        }

        is_running_ = true;
        spdlog::info("[GSCam] Pipeline running for: {}", pipeline_str_);
        return true;
    }

    hal::ImageFrame GStreamerCamera::get_frame() {
        // --- This is the main blocking call for the Core loop ---
        std::unique_lock<std::mutex> lock(mtx_);

        // Wait until 'on_new_sample' signals a frame is ready
        if (!cv_.wait_for(lock, std::chrono::milliseconds(100), [this]{ return new_frame_available_.load(); })) {
            // Timeout - sensor might have disconnected
            spdlog::warn("[GSCam] GetFrame timeout! No data from camera.");
            return {0}; // Return invalid frame
        }

        // We have a frame
        hal::ImageFrame frame;
        frame.timestamp = last_timestamp_;
        frame.width = width_;
        frame.height = height_;
        frame.stride = width_ * 3; // RGB = 3 bytes
        frame.data_ptr = pinned_buffer_; // The pointer to ZERO-COPY memory

        new_frame_available_ = false; // Consume the frame
        return frame;
    }

    // --- STATIC CALLBACKS (C-style, called from GStreamer thread) ---

    gboolean GStreamerCamera::on_bus_message(GstBus *bus, GstMessage *msg, GStreamerCamera *self) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *debug;
                gst_message_parse_error(msg, &err, &debug);
                spdlog::error("[GSCam] Bus Error: {} ({})", err->message, debug);
                g_clear_error(&err);
                g_free(debug);
                self->is_running_ = false;
                break;
            }
            case GST_MESSAGE_EOS: // End of Stream
                spdlog::info("[GSCam] End of Stream received.");
                self->is_running_ = false;
                break;
            default:
                break;
        }
        return TRUE; // Keep watch active
    }

    GstFlowReturn GStreamerCamera::on_new_sample(GstElement *sink, GStreamerCamera *self) {
        GstSample *sample = nullptr;
        g_signal_emit_by_name(sink, "pull-sample", &sample);

        if (!sample) {
            return GST_FLOW_ERROR;
        }

        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstCaps *caps = gst_sample_get_caps(sample);
        GstStructure *s = gst_caps_get_structure(caps, 0);

        // --- Frame Metadata ---
        int w, h;
        gst_structure_get_int(s, "width", &w);
        gst_structure_get_int(s, "height", &h);
        size_t current_buffer_size = w * h * 3; // RGB

        // --- CRITICAL SECTION: Update Shared Buffer ---
        {
            std::lock_guard<std::mutex> lock(self->mtx_);

            // Allocate or re-allocate pinned buffer if resolution changes
            if (!self->pinned_buffer_ || self->buffer_size_ != current_buffer_size) {
                if (self->pinned_buffer_) platform::CudaAllocator::free_pinned(self->pinned_buffer_);

                self->width_ = w;
                self->height_ = h;
                self->buffer_size_ = current_buffer_size;
                self->pinned_buffer_ = (uint8_t*)platform::CudaAllocator::alloc_pinned(self->buffer_size_);
                spdlog::info("[GSCam] Allocated {}x{} Zero-Copy buffer", w, h);
            }

            // Map GStreamer buffer for reading
            GstMapInfo map;
            gst_buffer_map(buffer, &map, GST_MAP_READ);

            // Copy data directly into the GPU-accessible pinned memory
            std::memcpy(self->pinned_buffer_, map.data, map.size);

            gst_buffer_unmap(buffer, &map);

            self->last_timestamp_ = std::chrono::system_clock::now().time_since_epoch().count() / 1e9;
            self->new_frame_available_ = true;
        }

        gst_sample_unref(sample);

        // --- Wake up the get_frame() call ---
        self->cv_.notify_one();

        return GST_FLOW_OK;
    }
}