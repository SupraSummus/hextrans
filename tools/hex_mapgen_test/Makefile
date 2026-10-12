# Standalone self-test for the map generation shapes in
# src/simutrans/dataobj/mapgen_shape.h and physical_dot_x2 in
# src/simutrans/dataobj/koord.h, built against them and
# src/simutrans/dataobj/koord_hex.cc directly.  Not wired into the
# main build.
#
#   make           # build ./hex_mapgen_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_mapgen_test.cc ../../src/simutrans/dataobj/koord_hex.cc
HEADERS   = ../../src/simutrans/dataobj/mapgen_shape.h ../../src/simutrans/dataobj/koord.h

hex_mapgen_test: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_mapgen_test
	./hex_mapgen_test

clean:
	rm -f hex_mapgen_test
