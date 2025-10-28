#!/bin/sh
# Wrapper to generate enums.cpp and enums.hpp
# Args: python3 enums.py doxy_xml build_dir output_cpp output_hpp

PYTHON="$1"
SCRIPT="$2"
DOXY_XML="$3"
BUILD_DIR="$4"
OUTPUT_CPP="$5"
OUTPUT_HPP="$6"

# Run enums.py and capture stdout to enums.cpp
"$PYTHON" "$SCRIPT" "$DOXY_XML" "$BUILD_DIR" > "$OUTPUT_CPP"

# Move generated enums.hpp to the output location
mv "$BUILD_DIR/enums.hpp" "$OUTPUT_HPP"
