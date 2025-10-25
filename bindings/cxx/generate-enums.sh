#!/bin/sh
# Generate enums.cpp and enums.hpp
# Args: python3 enums.py xml_index_file
python3 "$1" "$2" > enums.cpp
