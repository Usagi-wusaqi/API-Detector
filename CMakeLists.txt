cmake_minimum_required(VERSION 3.16)
project(api-detector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Concurrent Sql)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(GUI_SOURCES
    gui/gui_main.cpp
    gui/main_window.cpp
    gui/api_input_widget.cpp
    gui/result_widget.cpp
    gui/history_widget.cpp
    gui/settings_widget.cpp
    gui/checker_thread.cpp
)

set(GUI_HEADERS
    gui/main_window.h
    gui/api_input_widget.h
    gui/result_widget.h
    gui/history_widget.h
    gui/settings_widget.h
    gui/checker_thread.h
)

set(CORE_SOURCES
    src/api_checker.cpp
    src/http_client.cpp
    src/file_utils.cpp
    src/config_manager.cpp
)

set(LAUNCHER_SOURCES
    launcher/launcher_main.cpp
    launcher/launcher_window.cpp
)

set(LAUNCHER_HEADERS
    launcher/launcher_window.h
)

add_executable(api-checker-gui
    ${GUI_SOURCES}
    ${GUI_HEADERS}
    ${CORE_SOURCES}
    gui/resources.qrc
)

target_include_directories(api-checker-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(api-checker-gui PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql
    Threads::Threads
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(api-checker-gui PRIVATE nlohmann_json::nlohmann_json)
else()
    target_include_directories(api-checker-gui PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
endif()

if(WIN32)
    set_target_properties(api-checker-gui PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if(MSVC)
    target_compile_options(api-checker-gui PRIVATE /W4 /O2 /utf-8)
else()
    target_compile_options(api-checker-gui PRIVATE -Wall -Wextra -O3 -march=native)
endif()

add_executable(api-detector-launcher
    ${LAUNCHER_SOURCES}
    ${LAUNCHER_HEADERS}
)

target_include_directories(api-detector-launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(api-detector-launcher PRIVATE
    Qt6::Core
    Qt6::Widgets
)

if(WIN32)
    set_target_properties(api-detector-launcher PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if(MSVC)
    target_compile_options(api-detector-launcher PRIVATE /W4 /O2 /utf-8)
else()
    target_compile_options(api-detector-launcher PRIVATE -Wall -Wextra -O3 -march=native)
endif()

install(TARGETS api-checker-gui api-detector-launcher
    RUNTIME DESTINATION bin
)
