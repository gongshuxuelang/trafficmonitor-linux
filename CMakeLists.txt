cmake_minimum_required(VERSION 3.28)
project(trafficmonitor_linux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 项目未使用 C++ 模块。关闭模块扫描可去掉 -fmodules-ts 等参数，
# 让 ccache 能正常缓存编译（ccache 不支持模块映射相关参数）。
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_compile_options(
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-Wall>
    $<$<CONFIG:Debug>:-Wextra>
)
add_compile_definitions($<$<CONFIG:Debug>:TM_DEBUG>)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-pipe)
endif()

option(TM_USE_CCACHE "Use ccache as compiler launcher when available" ON)
if(TM_USE_CCACHE)
    find_program(TM_CCACHE_EXECUTABLE ccache)
    if(TM_CCACHE_EXECUTABLE)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${TM_CCACHE_EXECUTABLE}")
        message(STATUS "Using ccache: ${TM_CCACHE_EXECUTABLE}")
    endif()
endif()

option(TM_SEPARATE_DEBUG_SYMBOLS "Split .debug files for Release binaries" OFF)
if(TM_SEPARATE_DEBUG_SYMBOLS)
    add_compile_options($<$<CONFIG:Release>:-g>)
endif()

function(tm_separate_debug_symbols target)
    if(NOT TM_SEPARATE_DEBUG_SYMBOLS)
        return()
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        return()
    endif()
    if(NOT CMAKE_OBJCOPY)
        message(WARNING "CMAKE_OBJCOPY not found; skipping debug symbol split for ${target}")
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --only-keep-debug
                $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>.debug
        COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:${target}>
        COMMAND ${CMAKE_OBJCOPY}
                --add-gnu-debuglink=$<TARGET_FILE:${target}>.debug
                $<TARGET_FILE:${target}>
        COMMENT "Splitting debug symbols for ${target}"
        VERBATIM
    )
endfunction()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Widgets Sql Network Test)

add_library(tm_core STATIC
    src/core/collectors/network_collector.cpp
    src/core/collectors/cpu_collector.cpp
    src/core/collectors/memory_collector.cpp
    src/core/collectors/disk_collector.cpp
    src/core/collectors/temperature_collector.cpp
    src/core/collectors/cpu_frequency_collector.cpp
    src/core/system_metrics.cpp
    src/core/storage/storage.cpp
    src/core/plugin/plugin_host.cpp
    src/core/formatting.cpp
    src/core/app_settings.cpp
    src/core/sampler.cpp
    src/core/logger.cpp
)
target_include_directories(tm_core PUBLIC include)
target_link_libraries(tm_core PUBLIC Qt6::Core Qt6::Sql ${CMAKE_DL_LIBS})

add_executable(tm_tests
    tests/unit/test_network_collector.cpp
)
target_link_libraries(tm_tests PRIVATE tm_core Qt6::Test)

add_executable(test_cpu_collector
    tests/unit/test_cpu_collector.cpp
)
target_link_libraries(test_cpu_collector PRIVATE tm_core Qt6::Test)

add_executable(test_memory_collector
    tests/unit/test_memory_collector.cpp
)
target_link_libraries(test_memory_collector PRIVATE tm_core Qt6::Test)

add_executable(test_disk_collector
    tests/unit/test_disk_collector.cpp
)
target_link_libraries(test_disk_collector PRIVATE tm_core Qt6::Test)

add_executable(test_temperature_collector
    tests/unit/test_temperature_collector.cpp
)
target_link_libraries(test_temperature_collector PRIVATE tm_core Qt6::Test)

add_executable(test_cpu_frequency_collector
    tests/unit/test_cpu_frequency_collector.cpp
)
target_link_libraries(test_cpu_frequency_collector PRIVATE tm_core Qt6::Test)

add_executable(test_system_metrics
    tests/unit/test_system_metrics.cpp
)
target_link_libraries(test_system_metrics PRIVATE tm_core Qt6::Test)

add_executable(test_storage
    tests/unit/test_storage.cpp
)
target_link_libraries(test_storage PRIVATE tm_core Qt6::Test)

add_library(test_plugin MODULE tests/plugins/test_plugin.cpp)
target_include_directories(test_plugin PRIVATE include)
set_target_properties(test_plugin PROPERTIES PREFIX "" OUTPUT_NAME "test_plugin")

add_library(empty_plugin MODULE tests/plugins/empty_plugin.cpp)
set_target_properties(empty_plugin PROPERTIES PREFIX "" OUTPUT_NAME "empty_plugin")

add_library(gpu_plugin MODULE plugins/gpu/gpu_plugin.cpp)
target_include_directories(gpu_plugin PRIVATE include)
target_link_libraries(gpu_plugin PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(gpu_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gpu_plugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

add_executable(trafficmonitor-linux
    src/ui/main.cpp
    src/ui/floating_window.cpp
    src/ui/settings_dialog.cpp
    src/ui/tray_icon.cpp
)
target_include_directories(trafficmonitor-linux PRIVATE include src)
target_link_libraries(trafficmonitor-linux PRIVATE
    tm_core
    Qt6::Widgets
    Qt6::Network
)
add_dependencies(trafficmonitor-linux gpu_plugin)

add_executable(test_plugin_host
    tests/unit/test_plugin_host.cpp
)
target_link_libraries(test_plugin_host PRIVATE tm_core Qt6::Test)
target_compile_definitions(test_plugin_host PRIVATE
    TEST_PLUGIN_PATH="$<TARGET_FILE:test_plugin>"
    EMPTY_PLUGIN_PATH="$<TARGET_FILE:empty_plugin>"
)
add_dependencies(test_plugin_host test_plugin empty_plugin)

add_executable(test_gpu_plugin
    tests/unit/test_gpu_plugin.cpp
)
target_link_libraries(test_gpu_plugin PRIVATE tm_core Qt6::Test)
target_compile_definitions(test_gpu_plugin PRIVATE GPU_PLUGIN_PATH="$<TARGET_FILE:gpu_plugin>")
add_dependencies(test_gpu_plugin gpu_plugin)

add_executable(test_formatting
    tests/unit/test_formatting.cpp
)
target_link_libraries(test_formatting PRIVATE tm_core Qt6::Test)

add_executable(test_app_settings
    tests/unit/test_app_settings.cpp
)
target_link_libraries(test_app_settings PRIVATE tm_core Qt6::Test)

add_executable(test_floating_window
    tests/unit/test_floating_window.cpp
    src/ui/floating_window.cpp
)
target_include_directories(test_floating_window PRIVATE include src)
target_link_libraries(test_floating_window PRIVATE tm_core Qt6::Widgets Qt6::Test)

add_executable(test_sampler
    tests/unit/test_sampler.cpp
)
target_link_libraries(test_sampler PRIVATE tm_core Qt6::Test)

add_executable(test_settings_dialog
    tests/unit/test_settings_dialog.cpp
    src/ui/settings_dialog.cpp
)
target_include_directories(test_settings_dialog PRIVATE include src)
target_link_libraries(test_settings_dialog PRIVATE tm_core Qt6::Widgets Qt6::Test)

add_executable(test_tray_icon
    tests/unit/test_tray_icon.cpp
    src/ui/tray_icon.cpp
)
target_include_directories(test_tray_icon PRIVATE include src)
target_link_libraries(test_tray_icon PRIVATE tm_core Qt6::Widgets Qt6::Test)

add_executable(test_logger
    tests/unit/test_logger.cpp
)
target_link_libraries(test_logger PRIVATE tm_core Qt6::Test)

enable_testing()
add_test(NAME unit_tests COMMAND tm_tests)
add_test(NAME cpu_collector COMMAND test_cpu_collector)
add_test(NAME memory_collector COMMAND test_memory_collector)
add_test(NAME disk_collector COMMAND test_disk_collector)
add_test(NAME temperature_collector COMMAND test_temperature_collector)
add_test(NAME cpu_frequency_collector COMMAND test_cpu_frequency_collector)
add_test(NAME system_metrics COMMAND test_system_metrics)
add_test(NAME storage COMMAND test_storage)
add_test(NAME plugin_host COMMAND test_plugin_host)
add_test(NAME gpu_plugin COMMAND test_gpu_plugin)
add_test(NAME formatting COMMAND test_formatting)
add_test(NAME app_settings COMMAND test_app_settings)
add_test(NAME floating_window COMMAND test_floating_window)
set_tests_properties(floating_window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME sampler COMMAND test_sampler)
add_test(NAME settings_dialog COMMAND test_settings_dialog)
set_tests_properties(settings_dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME tray_icon COMMAND test_tray_icon)
set_tests_properties(tray_icon PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME logger COMMAND test_logger)
add_test(NAME app_smoke COMMAND trafficmonitor-linux --smoke-test)
set_tests_properties(app_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-config;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-data;TRAFFICMONITOR_PLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins"
    TIMEOUT 15
)

tm_separate_debug_symbols(trafficmonitor-linux)
tm_separate_debug_symbols(gpu_plugin)

install(TARGETS trafficmonitor-linux RUNTIME DESTINATION bin)
install(TARGETS gpu_plugin LIBRARY DESTINATION lib/trafficmonitor-linux/plugins)
install(PROGRAMS packaging/trafficmonitor-linux-wrapper DESTINATION bin)
install(FILES packaging/deb/trafficmonitor-linux.desktop DESTINATION share/applications)
