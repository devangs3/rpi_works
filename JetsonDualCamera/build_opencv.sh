#!/bin/bash
mkdir -p build_opencv
cd build_opencv
cmake ..
make perf_opencv
echo "OpenCV profiling binary built inside build_opencv/perf_opencv"
