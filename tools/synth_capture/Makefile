# Standalone capture tool that dumps the engine's synth ground tile
# rasteriser output as PPM, for the parametric pipeline on the pakset
# side (`models/parametric/ground/refs/` in hextrans-pak128).
#
# Includes synth_ground_raster.h directly — header-only, depends only
# on the other header-only synth helpers (synth_geometry.h,
# synth_plane_partition.h) and a few pure-typedef headers (simcolor.h,
# simtypes.h, ribi.h, hex_proj.h).  No image_t, no simgraph linkage.
#
#   make           # build ./synth_capture
#   make run       # build and dump refs into ./out/
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES ?= -I ../../src

SRCS = synth_capture.cc \
       ../../src/simutrans/descriptor/synth_ground_raster.h \
       ../../src/simutrans/descriptor/synth_geometry.h \
       ../../src/simutrans/descriptor/synth_plane_partition.h

synth_capture: $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ synth_capture.cc

.PHONY: run clean
run: synth_capture
	mkdir -p out
	./synth_capture out

clean:
	rm -f synth_capture
	rm -rf out
