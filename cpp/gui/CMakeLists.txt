add_executable(poe-bot WIN32
    src/main.cpp
    src/app.cpp
    src/d3d11_backend.cpp
    src/main_window.cpp
    src/capture_service.cpp
    src/affix_library_widget.cpp
    src/hotkey_capture.cpp
    src/panels/main_layout.cpp
    src/panels/config_panel.cpp
    src/panels/craft_panel.cpp
    src/panels/map_panel.cpp
    src/panels/deposit_panel.cpp
    src/panels/log_panel.cpp
    res/app.rc
)

target_include_directories(poe-bot
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# rc.exe needs the resource header reachable the same way the C++ sources see
# it (via poebot/gui/resource.h), and the .ico sits next to app.rc.
target_include_directories(poe-bot PRIVATE
    "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_SOURCE_DIR}/res>"
)

target_link_libraries(poe-bot
    PRIVATE
        poebot-core
        imgui::imgui
        spdlog::spdlog
        d3d11
        dxgi
        d3dcompiler
        dwmapi
)

set_target_properties(poe-bot PROPERTIES
    OUTPUT_NAME "poe-bot"
)
