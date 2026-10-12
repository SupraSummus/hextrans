# Standalone self-test for map_shape_t in
# src/simutrans/dataobj/map_shape.h and the map rotations built on it
# (koord3d.h, tpl/array2d_tpl.h), built against them and
# dataobj/koord_hex.cc directly.  Not wired into the main build.
#
#   make           # build ./hex_map_shape_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_map_shape_test.cc ../../src/simutrans/dataobj/koord_hex.cc
HEADERS   = ../../src/simutrans/dataobj/map_shape.h ../../src/simutrans/dataobj/koord.h \
            ../../src/simutrans/dataobj/koord3d.h ../../src/simutrans/tpl/array2d_tpl.h

hex_map_shape_test: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_map_shape_test
	./hex_map_shape_test

clean:
	rm -f hex_map_shape_test
