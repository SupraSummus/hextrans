# Standalone self-test for the in-tile vehicle offsets in
# src/simutrans/vehicle/screen_chord.h, checked against the projection in
# src/simutrans/display/hex_proj.h.  Not wired into the main build.
#
#   make           # build ./hex_chord_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_chord_test.cc ../../src/simutrans/dataobj/koord_hex.cc
HEADERS   = ../../src/simutrans/vehicle/screen_chord.h \
            ../../src/simutrans/display/hex_proj.h \
            ../../src/simutrans/dataobj/koord.h \
            ../../src/simutrans/dataobj/ribi.h \
            ../../src/simutrans/simunits.h \
            ../../src/simutrans/simconst.h

hex_chord_test: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_chord_test
	./hex_chord_test

clean:
	rm -f hex_chord_test
