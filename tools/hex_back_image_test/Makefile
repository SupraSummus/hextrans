# Standalone self-test for the back-image wall-floor decoder in
# src/simutrans/ground/back_image_decode.h.  Not wired into the main
# build.
#
# Header-only inline pure function — the test includes it directly,
# no duplicated bodies.
#
#   make           # build ./hex_back_image_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src

hex_back_image_test: hex_back_image_test.cc ../../src/simutrans/ground/back_image_decode.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ hex_back_image_test.cc

.PHONY: run clean
run: hex_back_image_test
	./hex_back_image_test

clean:
	rm -f hex_back_image_test
