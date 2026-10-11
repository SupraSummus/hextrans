# Standalone self-test for the hex projection helpers in
# src/simutrans/display/hex_proj.h, built against the header and
# src/simutrans/dataobj/koord_hex.cc directly.  Not wired into the
# main build.
#
#   make           # build ./hex_proj_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_proj_test.cc ../../src/simutrans/dataobj/koord_hex.cc

hex_proj_test: $(SOURCES) ../../src/simutrans/display/hex_proj.h ../../src/simutrans/dataobj/koord.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_proj_test
	./hex_proj_test

clean:
	rm -f hex_proj_test
