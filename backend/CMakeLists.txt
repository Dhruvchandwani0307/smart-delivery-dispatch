cmake_minimum_required(VERSION 3.15)
project(SmartDeliveryDispatch CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Core business-logic library (models + managers + services).
# Kept separate from main.cpp / the HTTP layer so it can also be linked into
# the test executable without duplicating compilation.
# ---------------------------------------------------------------------------
add_library(dispatch_core
    src/services/DispatchEngine.cpp
)
target_include_directories(dispatch_core PUBLIC include)
target_compile_features(dispatch_core PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# Main server executable
# ---------------------------------------------------------------------------
add_executable(dispatch_server src/main.cpp)
target_link_libraries(dispatch_server PRIVATE dispatch_core Threads::Threads)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(../tests tests_build)
