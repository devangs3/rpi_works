cmake_minimum_required(VERSION 3.12)
project(JetsonCameraProfiling)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find both dependencies globally
find_package(OpenCV REQUIRED COMPONENTS core videoio highgui imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

# Target 1: OpenCV Implementation
add_executable(perf_opencv src_opencv.cpp)
target_link_libraries(perf_opencv PRIVATE opencv_core opencv_videoio opencv_highgui opencv_imgproc)

# Target 2: Native GStreamer Implementation
add_executable(perf_native src_native.cpp)
target_include_directories(perf_native PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(perf_native PRIVATE ${GSTREAMER_LIBRARIES})
