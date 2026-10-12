# Standalone self-test for the sprite-slot table in
# src/simutrans/descriptor/sprite_dir.h, checked against the projection in
# src/simutrans/display/hex_proj.h.  Not wired into the main build.
#
#   make           # build ./hex_dir_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_dir_test.cc ../../src/simutrans/dataobj/koord_hex.cc
HEADERS   = ../../src/simutrans/descriptor/sprite_dir.h \
            ../../src/simutrans/display/hex_proj.h \
            ../../src/simutrans/dataobj/koord.h \
            ../../src/simutrans/dataobj/ribi.h

hex_dir_test: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_dir_test
	./hex_dir_test

clean:
	rm -f hex_dir_test
