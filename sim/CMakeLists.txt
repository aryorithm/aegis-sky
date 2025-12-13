cmake_minimum_required(VERSION 3.20)
project(aegis_sim)

# Find Dependencies
find_package(glm REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

# Include Directories
target_include_directories(aegis_sim PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Source Files (Added Physics)
add_executable(aegis_sim
    src/main.cpp
    src/engine/TimeManager.cpp
    src/engine/SimEntity.cpp
    src/engine/SimEngine.cpp       # <--- THIS FILE
    src/engine/ScenarioLoader.cpp
    src/physics/DroneDynamics.cpp
    src/physics/RadarPhysics.cpp
    src/bridge_server/ShmWriter.cpp
    src/phenomenology/optics/MockRenderer.cpp
)

# Link Libraries
target_link_libraries(aegis_sim PRIVATE 
    glm::glm 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_compile_features(aegis_sim PUBLIC cxx_std_20)