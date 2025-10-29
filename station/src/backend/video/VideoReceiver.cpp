#include "VideoReceiver.h"
#include <QDebug>
#include <QVideoFrame>

namespace aegis::station::video {

    VideoReceiver::VideoReceiver(QObject *parent) : QObject(parent) {
        // Initialize GStreamer Library
        gst_init(nullptr, nullptr);
    }

    VideoReceiver::~VideoReceiver() {
        stop();
    }

    void VideoReceiver::setVideoSink(QVideoSink* sink) {
        if (m_videoSink != sink) {
            m_videoSink = sink;
            emit videoSinkChanged();
        }
    }

    void VideoReceiver::setUri(const QString &uri) {
        if (m_uri != uri) {
            m_uri = uri;
            emit uriChanged();
            // Auto-restart if running
            if (m_pipeline) {
                stop();
                start();
            }
        }
    }

    void VideoReceiver::start() {
        if (m_pipeline) return;

        qInfo() << "[Video] Starting Pipeline for:" << m_uri;

        // ---------------------------------------------------------------------
        // LOW LATENCY PIPELINE DEFINITION
        // ---------------------------------------------------------------------
        // 1. udpsrc: Receive raw bytes
        // 2. application/x-rtp: Define protocol
        // 3. rtph265depay: Extract H.265 from packets
        // 4. avdec_h265: Decode to raw pixels
        // 5. videoconvert: Ensure format is RGB/RGBA for Qt
        // 6. appsink: Handover to C++
        // ---------------------------------------------------------------------
        
        QString pipelineStr;

        // Logic to switch between Test Pattern, UDP, or RTSP
        if (m_uri == "test") {
            pipelineStr = "videotestsrc pattern=ball ! video/x-raw,width=1280,height=720 ! videoconvert ! appsink name=sink";
        } 
        else if (m_uri.startsWith("udp://")) {
            // Extract port
            QString port = m_uri.split(":").last();
            pipelineStr = QString(
                "udpsrc port=%1 ! "
                "application/x-rtp, encoding-name=H265, payload=96 ! "
                "rtph265depay ! h265parse ! avdec_h265 ! "
                "videoconvert ! video/x-raw,format=RGBA ! "
                "appsink name=sink drop=true max-buffers=1" // DROP FRAMES FOR LATENCY
            ).arg(port);
        }

        GError *error = nullptr;
        m_pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

        if (error) {
            qCritical() << "[Video] Pipeline Error:" << error->message;
            emit errorOccurred(QString::fromUtf8(error->message));
            return;
        }

        // Get the AppSink element
        m_sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
        
        // Connect Callback
        g_object_set(m_sink, "emit-signals", TRUE, NULL);
        g_signal_connect(m_sink, "new-sample", G_CALLBACK(on_new_sample), this);

        // Start Playing
        gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    }

    void VideoReceiver::stop() {
        if (m_pipeline) {
            gst_element_set_state(m_pipeline, GST_STATE_NULL);
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
            m_sink = nullptr;
        }
    }

    // STATIC CALLBACK: Moves GStreamer buffer into Qt VideoSink
    GstFlowReturn VideoReceiver::on_new_sample(GstElement *sink, VideoReceiver *self) {
        GstSample *sample = nullptr;
        g_signal_emit_by_name(sink, "pull-sample", &sample);

        if (sample && self->m_videoSink) {
            GstBuffer *buffer = gst_sample_get_buffer(sample);
            GstCaps *caps = gst_sample_get_caps(sample);
            GstStructure *s = gst_caps_get_structure(caps, 0);

            int width, height;
            gst_structure_get_int(s, "width", &width);
            gst_structure_get_int(s, "height", &height);

            // Map the data
            GstMapInfo map;
            gst_buffer_map(buffer, &map, GST_MAP_READ);

            // Create QVideoFrame from raw bytes
            QVideoFrameFormat format(QSize(width, height), QVideoFrameFormat::Format_RGBA8888);
            QVideoFrame frame(format);
            
            if (frame.map(QVideoFrame::WriteOnly)) {
                memcpy(frame.bits(0), map.data, map.size);
                frame.unmap();
                
                // PUSH TO QML
                self->m_videoSink->setVideoFrame(frame);
            }

            gst_buffer_unmap(buffer, &map);
            gst_sample_unref(sample);
            return GST_FLOW_OK;
        }
        return GST_FLOW_ERROR;
    }
}