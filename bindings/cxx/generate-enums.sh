#!/bin/sh
# Wrapper to generate enums.cpp and enums.hpp
# Args: python3 enums.py doxy_xml build_dir output_cpp output_hpp

PYTHON="$1"
SCRIPT="$2"
DOXY_XML="$3"
BUILD_DIR="$4"
OUTPUT_CPP="$5"
OUTPUT_HPP="$6"

# Run enums.py - it writes enums.hpp to BUILD_DIR/enums.hpp and enums.cpp to stdout
"$PYTHON" "$SCRIPT" "$DOXY_XML" "$BUILD_DIR" > "$OUTPUT_CPP"

# enums.hpp is already at BUILD_DIR/enums.hpp which is where OUTPUT_HPP points
# No move needed since they're the same location
