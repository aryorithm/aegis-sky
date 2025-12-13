#include "models/AuraNet.h"
#include "data/SimDataset.h"
#include "xtorch/optim/AdamW.h"
#include "xtorch/loss/MSELoss.h"
#include <iostream>

using namespace aegis::brain;

int main() {
    std::cout << "[Brain] Initializing Training Pipeline..." << std::endl;

    // 1. Load Data
    // Assume Sim output is mounted at /mnt/data/sim_out
    data::SimDataset dataset("/mnt/data/sim_out");
    auto loader = xtorch::data::DataLoader(dataset, /*batch_size=*/32, /*shuffle=*/true);

    // 2. Init Model (Moves to GPU automatically if xTorch configured)
    models::AuraNet model;
    model.to(xtorch::kCUDA);

    // 3. Optimizer
    xtorch::optim::AdamW optimizer(model.parameters(), /*lr=*/0.001);

    // 4. Training Loop
    int epochs = 50;
    for (int epoch = 0; epoch < epochs; ++epoch) {
        float total_loss = 0.0f;

        for (auto& batch : loader) {
            auto [inputs, targets] = batch;
            inputs = inputs.to(xtorch::kCUDA);
            targets = targets.to(xtorch::kCUDA);

            // Forward
            auto preds = model.forward(inputs);

            // Loss Calculation (Simple MSE for BBox)
            auto loss = xtorch::loss::mse_loss(preds, targets);

            // Backward
            optimizer.zero_grad();
            loss.backward();
            optimizer.step();

            total_loss += loss.item<float>();
        }

        std::cout << "Epoch [" << epoch << "/" << epochs
                  << "] Loss: " << total_loss / loader.size() << std::endl;

        // Checkpoint
        if (epoch % 10 == 0) {
            xtorch::save(model, "checkpoints/aura_v1_ep" + std::to_string(epoch) + ".pth");
        }
    }

    // 5. Final Save
    xtorch::save(model, "aura_final.pth");
    std::cout << "[Brain] Training Complete. Model saved." << std::endl;
    return 0;
}