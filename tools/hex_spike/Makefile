# Standalone build for the hex-grid spike. Not wired into the main build.
#
#   make           # build ./hex_spike
#   make run       # build, run, dump hex_spike.ppm
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

hex_spike: hex_spike.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

.PHONY: run clean
run: hex_spike
	./hex_spike

clean:
	rm -f hex_spike hex_spike.ppm
