# Standalone self-test for the per-vertex storage helpers in
# src/simutrans/dataobj/koord_hex.cc, built against that file directly.
# Not wired into the main build.
#
#   make           # build ./hex_vertex_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_vertex_test.cc ../../src/simutrans/dataobj/koord_hex.cc

hex_vertex_test: $(SOURCES) ../../src/simutrans/dataobj/koord.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_vertex_test
	./hex_vertex_test

clean:
	rm -f hex_vertex_test
