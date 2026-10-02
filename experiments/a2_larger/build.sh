#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build/a2-larger
g++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Iinclude \
  experiments/a2_larger/worker.cpp src/*/*.cpp -o build/a2-larger/worker.tmp
mv build/a2-larger/worker.tmp build/a2-larger/worker
sha256sum build/a2-larger/worker
