# Standalone self-test for the per-vertex storage helpers added to
# src/simutrans/dataobj/koord.{h,cc}.  Not wired into the main build.
#
# Following the tools/hex_spike pattern: the test duplicates the
# helper implementations rather than linking the full koord.cc (which
# drags in loadsave / scr_coord / simrandom).  Keep the two copies in
# sync — the canonical source of truth is koord.cc; this tool exists
# to exercise the invariants that would otherwise only be testable
# through a pakset-running scenario test.
#
#   make           # build ./hex_vertex_test
#   make run       # build and run the self-test
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

hex_vertex_test: hex_vertex_test.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

.PHONY: run clean
run: hex_vertex_test
	./hex_vertex_test

clean:
	rm -f hex_vertex_test
