add_library(poebot-core STATIC
    src/config/profile.cpp
    src/config/settings.cpp
    src/config/settings_io.cpp
    src/log/log_sink.cpp
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
