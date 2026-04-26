# Standalone reproducer for the terrain save/load corruption in the hex port.
# See terrain_rdwr_test.cc for a full description of both bugs.
#
# The test currently FAILS — it demonstrates the regression that needs fixing.
# Once ground/grund.cc's restore-grid block is ported to the hex-aware
# per-vertex API, this should pass.
#
#   make           # build ./terrain_rdwr_test
#   make run       # build and run (non-zero exit = bugs still present)
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

terrain_rdwr_test: terrain_rdwr_test.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

.PHONY: run clean
run: terrain_rdwr_test
	./terrain_rdwr_test

clean:
	rm -f terrain_rdwr_test
