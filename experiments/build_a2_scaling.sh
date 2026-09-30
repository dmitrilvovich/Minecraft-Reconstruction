#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/a2-scaling
# Same Release optimization as the CMake target; no package installation needed.
c++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Iinclude \
  experiments/a2_scaling.cpp src/*/*.cpp -o build/a2-scaling/mcr_a2_scaling.tmp
mv build/a2-scaling/mcr_a2_scaling.tmp build/a2-scaling/mcr_a2_scaling
