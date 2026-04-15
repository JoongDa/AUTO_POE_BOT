add_executable(poe-bot WIN32
    src/main.cpp
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
