cmake_minimum_required(VERSION 3.16)

# ─── Source files ──────────────────────────────────────────────────────────────
set(VECU_SOURCES
    src/CANFrame.cpp
    src/CANBus.cpp
    src/VirtualECU.cpp
    src/EngineECU.cpp
    src/BrakeECU.cpp
    src/BatteryECU.cpp
    src/SteeringECU.cpp
    src/GatewayECU.cpp
    src/VehicleModel.cpp
    src/DigitalTwin.cpp
    src/FaultEngine.cpp
    src/FaultDetector.cpp
    src/Logger.cpp
)

# ─── Library (shared between simulator and tests) ──────────────────────────────
add_library(vecu_lib STATIC ${VECU_SOURCES})

target_include_directories(vecu_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(vecu_lib PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Threads (required for std::thread, std::mutex, etc.)
find_package(Threads REQUIRED)
target_link_libraries(vecu_lib PUBLIC Threads::Threads)

# ─── Simulator executable ──────────────────────────────────────────────────────
add_executable(vecu_sim src/main.cpp)
target_link_libraries(vecu_sim PRIVATE vecu_lib)
target_include_directories(vecu_sim PRIVATE include)

# Enable filesystem on older GCC
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(vecu_sim PRIVATE stdc++fs)
    target_link_libraries(vecu_lib PRIVATE stdc++fs)
endif()

# ─── GoogleTest (fetched automatically — no manual install required) ────────────
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Prevent GoogleTest from overriding our compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ─── Test executable ───────────────────────────────────────────────────────────
add_executable(vecu_tests
    tests/test_can_frame.cpp
    tests/test_can_bus.cpp
    tests/test_ecu.cpp
    tests/test_vehicle_model.cpp
    tests/test_digital_twin.cpp
    tests/test_fault_engine.cpp
)

target_link_libraries(vecu_tests PRIVATE
    vecu_lib
    GTest::gtest_main
    GTest::gmock_main
)

target_include_directories(vecu_tests PRIVATE include)

include(GoogleTest)
gtest_discover_tests(vecu_tests)
