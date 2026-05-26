cmake_minimum_required(VERSION 3.12)
project(JetsonCameraProfiling)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)


# 1. Find both dependencies globally
find_package(OpenCV REQUIRED COMPONENTS core videoio highgui imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
find_package(Threads REQUIRED)

# Target 1: OpenCV Implementation
# Explicitly use the root directory variable
add_executable(perf_opencv ${CMAKE_CURRENT_SOURCE_DIR}/src/src_opencv.cpp)
target_include_directories(perf_opencv PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(perf_opencv PRIVATE ${OpenCV_LIBS} Threads::Threads)

# Target 2: Native GStreamer Implementation
# Explicitly use the root directory variable
add_executable(perf_native ${CMAKE_CURRENT_SOURCE_DIR}/src/src_native.cpp)
target_include_directories(perf_native PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_directories(perf_native PRIVATE ${GSTREAMER_LIBRARY_DIRS})
target_link_libraries(perf_native PRIVATE ${GSTREAMER_LIBRARIES} Threads::Threads)

