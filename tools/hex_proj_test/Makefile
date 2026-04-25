# Standalone self-test for the hex projection helpers in
# src/simutrans/display/hex_proj.h.  Not wired into the main build.
#
# The test includes hex_proj.h directly — the helpers are header-only
# and pure (the only dependency is koord, also header-only).  No
# duplicated bodies to keep in sync.
#
#   make           # build ./hex_proj_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src

hex_proj_test: hex_proj_test.cc ../../src/simutrans/display/hex_proj.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ hex_proj_test.cc

.PHONY: run clean
run: hex_proj_test
	./hex_proj_test

clean:
	rm -f hex_proj_test
