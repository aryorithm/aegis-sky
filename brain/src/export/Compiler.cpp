#include "models/AuraNet.h"
#include "xinfer/builder/NetworkBuilder.h" // Your inference library builder
#include "xtorch/io/Serializer.h"
#include <iostream>

using namespace aegis::brain;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./compiler <path_to_model.pth>" << std::endl;
        return -1;
    }

    std::cout << "[Brain] Compiling AuraNet for Aegis Core..." << std::endl;

    // 1. Load Trained Weights
    models::AuraNet model;
    xtorch::load(model, argv[1]);

    // 2. Initialize xInfer Builder
    // Targeted for Jetson AGX Orin (FP16 Mode)
    xinfer::NetworkBuilder builder;
    builder.set_platform(xinfer::Platform::JETSON_ORIN);
    builder.set_precision(xinfer::Precision::FP16);

    // 3. Define the Network Graph for Inference
    // We recreate the structure, injecting the trained weights
    auto input = builder.add_input("input", {1, 5, 1080, 1920}); // 5 Channels!

    // Transfer weights from xTorch tensor to xInfer constant
    auto w_c1 = model.conv1_->weight.data_ptr<float>();
    auto layer1 = builder.add_conv2d(input, 64, 7, 2, w_c1);

    // ... (Repeat for all layers, automating this via graph traversal in production) ...
    // For MVP, we manually map the head:

    auto output = builder.add_output("detections");

    // 4. Compile
    std::cout << "[Brain] Optimizing TensorRT Engine..." << std::endl;
    builder.build_engine("aura_v1.plan");

    std::cout << "[Brain] Success! 'aura_v1.plan' is ready for upload to Core." << std::endl;
    return 0;
}