cmake_minimum_required(VERSION 3.16)
project(emg_control VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
    set(PLATFORM_LIBS ws2_32)               # Winsock2
else()
    set(PLATFORM_LIBS pthread)
endif()

# Compiler options and performance optimization
if(MSVC)
    add_compile_options(/W4 /permissive- /O2 /Oi /Ot)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -funroll-loops)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(LIB_SOURCES
    src/logger.cpp
    src/fake_emg_reader.cpp
    src/signal_processor.cpp
    src/feature_extractor.cpp
    src/mock_ml_model.cpp
    src/neural_net_model.cpp
    src/decision_engine.cpp
    src/command_packet.cpp
    src/udp_sender.cpp
    src/config.cpp
    src/pipeline.cpp
    src/serial_emg_reader.cpp
)

# Main library
add_library(emg_control_lib STATIC ${LIB_SOURCES})
target_link_libraries(emg_control_lib PUBLIC ${PLATFORM_LIBS})

# Main executable
add_executable(emg_control src/main.cpp)
target_link_libraries(emg_control PRIVATE emg_control_lib)

# Testing
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()

    # Test executable
    set(TEST_SOURCES
        tests/test_main.cpp
        tests/test_ring_buffer.cpp
        tests/test_thread_safe_queue.cpp
        tests/test_signal_processor.cpp
        tests/test_feature_extractor.cpp
        tests/test_mock_ml.cpp
        tests/test_neural_net.cpp
        tests/test_decision_engine.cpp
        tests/test_command_packet.cpp
        tests/test_udp_sender.cpp
        tests/test_pipeline.cpp
    )

    add_executable(emg_tests ${TEST_SOURCES})
    target_link_libraries(emg_tests PRIVATE emg_control_lib)

    add_test(NAME emg_tests COMMAND emg_tests)
endif()
