# Standalone self-test for slope_t::raise_base in
# src/simutrans/dataobj/ribi.h.  Not wired into the main build.
#
# Header-only inline pure function — the test includes it directly,
# no duplicated bodies.
#
#   make           # build ./hex_slope_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src

hex_slope_test: hex_slope_test.cc ../../src/simutrans/dataobj/ribi.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ hex_slope_test.cc

.PHONY: run clean
run: hex_slope_test
	./hex_slope_test

clean:
	rm -f hex_slope_test
