# Standalone self-test for the A* heuristic of way_builder_t::calc_distance,
# koord_distance_to_rect in src/simutrans/dataobj/koord.h, built against
# koord.h and dataobj/koord_hex.cc directly.  Not wired into the main build.
#
#   make           # build ./hex_route_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_route_test.cc ../../src/simutrans/dataobj/koord_hex.cc

hex_route_test: $(SOURCES) ../../src/simutrans/dataobj/koord.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_route_test
	./hex_route_test

clean:
	rm -f hex_route_test
