# Standalone self-test for the computer players' site-search geometry in
# src/simutrans/player/ai_site.h, built against the header and
# dataobj/koord_hex.cc directly.  Not wired into the main build.
#
#   make           # build ./hex_ai_site_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src
SOURCES   = hex_ai_site_test.cc ../../src/simutrans/dataobj/koord_hex.cc
HEADERS   = ../../src/simutrans/dataobj/koord.h ../../src/simutrans/player/ai_site.h

hex_ai_site_test: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

.PHONY: run clean
run: hex_ai_site_test
	./hex_ai_site_test

clean:
	rm -f hex_ai_site_test
