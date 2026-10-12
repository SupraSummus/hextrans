# Standalone self-test for the "tiles around here" helpers in
# src/simutrans/dataobj/koord.h (koord_distance_to_rect, hex_disc),
# built against the header and dataobj/koord_hex.cc directly.  Not wired
# into the main build.
#
#   make           # build ./hex_neighbourhood_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_neighbourhood_test.cc ../../src/simutrans/dataobj/koord_hex.cc
HEADERS   = ../../src/simutrans/dataobj/koord.h

hex_neighbourhood_test: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_neighbourhood_test
	./hex_neighbourhood_test

clean:
	rm -f hex_neighbourhood_test
