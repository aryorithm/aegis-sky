cmake_minimum_required(VERSION 3.20)
project(aegis_station LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# 1. Find Qt6 & GStreamer
find_package(Qt6 REQUIRED COMPONENTS Quick Widgets Network Gui)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0)

# 2. Add Sources
set(SOURCES
    src/main.cpp
    src/backend/app/StationApp.cpp
    src/backend/comms/CoreClient.cpp
    src/backend/video/VideoReceiver.cpp
)

# 3. Create Executable
add_executable(aegis_station ${SOURCES})

# 4. Link Libraries
target_link_libraries(aegis_station PRIVATE
    Qt6::Quick
    Qt6::Widgets
    Qt6::Network
    ${GST_LIBRARIES}
    aegis_shared  # Link to the Common Protocol definitions
)

# 5. Embed QML files into the binary (Resource System)
qt_add_qml_module(aegis_station
    URI Aegis.Station
    VERSION 1.0
    QML_FILES
        src/frontend/Main.qml
        src/frontend/views/TacticalView.qml
        src/frontend/components/VideoHud.qml
)