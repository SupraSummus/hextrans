# Standalone self-test for the relief sampler in
# src/simutrans/dataobj/height_field_sample.h, built against it and
# dataobj/koord_hex.cc directly.  Not wired into the main build.
#
#   make           # build ./hex_heightfield_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_heightfield_test.cc ../../src/simutrans/dataobj/koord_hex.cc

hex_heightfield_test: $(SOURCES) ../../src/simutrans/dataobj/height_field_sample.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_heightfield_test
	./hex_heightfield_test

clean:
	rm -f hex_heightfield_test
