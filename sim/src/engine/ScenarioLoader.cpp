#include "sim/engine/ScenarioLoader.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace aegis::sim::engine {

    using json = nlohmann::json;

    // Helper to convert string to Enum
    EntityType string_to_type(const std::string& s) {
        if (s == "QUADCOPTER") return EntityType::QUADCOPTER;
        if (s == "FIXED_WING") return EntityType::FIXED_WING;
        if (s == "BIRD") return EntityType::BIRD;
        return EntityType::UNKNOWN;
    }

    std::vector<std::shared_ptr<SimEntity>> ScenarioLoader::load_mission(const std::string& filepath) {
        std::vector<std::shared_ptr<SimEntity>> entities;
        std::ifstream file(filepath);
        
        if (!file.is_open()) {
            spdlog::error("[Loader] File not found: {}", filepath);
            return entities;
        }

        try {
            json j;
            file >> j;
            spdlog::info("[Loader] Scenario: {}", j["mission_name"].get<std::string>());

            for (const auto& item : j["entities"]) {
                std::string name = item["name"];
                
                auto pos_arr = item["start_pos"];
                glm::dvec3 pos(pos_arr[0], pos_arr[1], pos_arr[2]);

                auto entity = std::make_shared<SimEntity>(name, pos);

                // Set Properties
                if (item.contains("type")) entity->set_type(string_to_type(item["type"]));
                if (item.contains("rcs")) entity->set_rcs(item["rcs"]);
                if (item.contains("speed")) entity->set_speed(item["speed"]);

                // Load Waypoints
                if (item.contains("waypoints")) {
                    for (const auto& wp : item["waypoints"]) {
                        entity->add_waypoint(glm::dvec3(wp[0], wp[1], wp[2]));
                    }
                }

                entities.push_back(entity);
            }
        }
        catch (const std::exception& e) {
            spdlog::error("[Loader] JSON Error: {}", e.what());
        }

        return entities;
    }
}