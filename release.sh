#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build

# Configure and build
cd build
cmake ..
make

# Create package
make package

echo "Release package created in release/"
