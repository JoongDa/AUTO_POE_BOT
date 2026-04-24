add_library(poebot-core STATIC
    src/config/profile.cpp
    src/config/settings.cpp
    src/config/settings_io.cpp
    src/config/affix_library.cpp
    src/i18n/i18n.cpp
    src/log/log_sink.cpp
    src/sys/encoding.cpp
    src/sys/clipboard.cpp
    src/win/window.cpp
    src/input/cursor.cpp
    src/input/mouse.cpp
    src/input/motion.cpp
    src/input/keyboard.cpp
    src/hotkey/hotkey_manager.cpp
    src/item/affix_matcher.cpp
    src/task/task_runner.cpp
    src/task/deposit_task.cpp
    src/task/craft_task.cpp
    src/task/map_task.cpp
)

target_include_directories(poebot-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(poebot-core PUBLIC cxx_std_20)

target_link_libraries(poebot-core
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)
