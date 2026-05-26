#!/bin/bash
mkdir -p build_native
cd build_native
cmake ..
make perf_native
echo "Native profiling binary built inside build_native/perf_native"
