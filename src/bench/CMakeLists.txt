#
# This file is part of the Simutrans project under the artistic licence.
# (see licence.txt)
#

# bench_pak: perf-test rig for pakset loading.  Drives the descriptor-read
# path (`load_pak_from_fp`) over every .pak in a directory and times it —
# see bench_pak.cc.
#
# Like fuzz_pak it needs the *real* registry of obj_reader_t subclasses
# (they self-register at static init via OBJ_READER_DEF), which means
# linking the full simutrans source set.  We inherit that set from the
# simutrans target rather than duplicate it, and skip simsys main() via
# SIMUTRANS_NO_MAIN (our own main wins).  Unlike the fuzz targets this is a
# plain executable: no libFuzzer, no sanitizer flags, builds under gcc or
# clang.  It forks per iteration rather than freeing descriptors
# in-process, so it needs no TRACK_DESCRIPTORS — the read populates fresh
# reader tables that die with the child.
#
# Representative-load note: the cost that dominates a real consumer pakset
# load is image processing — the per-pixel decode loop in image_reader.cc
# (gated on COLOUR_DEPTH != 0) plus simgraph16_register_image's player-colour
# scan.  A COLOUR_DEPTH=0 (SIMUTRANS_BACKEND=none) build skips all of it via
# `goto adjust_image`, so it under-measures the real load.  To bench what a
# consumer machine actually does, configure with SIMUTRANS_BACKEND=sdl2
# (COLOUR_DEPTH=16, real simgraph16 renderer).  We can't link simsys_s2 (it
# owns main() and needs an SDL display), so we swap the SDL windowing
# sources out of the inherited list and substitute the headless ones —
# simgraph16's register_image only populates software image tables, no
# display required, and simsys_posix provides the dr_textur* stubs it links
# against.  Backend=none still works and gives the pure decode+decompress
# number (no pixel work).

get_target_property(_SIM_SOURCES   simutrans SOURCES)
get_target_property(_SIM_INCLUDES  simutrans INCLUDE_DIRECTORIES)
get_target_property(_SIM_DEFS      simutrans COMPILE_DEFINITIONS)
get_target_property(_SIM_OPTS      simutrans COMPILE_OPTIONS)
get_target_property(_SIM_LIBS      simutrans LINK_LIBRARIES)

# Windowing / sound sources that pull in a display or own main(); the bench
# runs headless, so drop them and substitute the no-op headless backends.
# Matched on basename so the absolute-path re-rooting below is unaffected.
set(_BENCH_DROP
	simsys_s2.cc            # SDL2 windowing + main()
	simsys_w.cc             # GDI windowing + main()
	clipboard_s2.cc         # SDL2 clipboard
	sdl2_sound.cc           # SDL2 sound
)

# Sources are stored relative to the root CMakeLists; re-root to absolute
# paths so target_sources replays cleanly from src/bench (same as fuzz).
set(_SIM_SOURCES_ABS "")
foreach (_src IN LISTS _SIM_SOURCES)
	if (_src MATCHES "\\.(h|hpp)$")
		continue ()
	endif ()
	get_filename_component(_base "${_src}" NAME)
	if (_base IN_LIST _BENCH_DROP)
		continue ()
	endif ()
	if (IS_ABSOLUTE "${_src}")
		list(APPEND _SIM_SOURCES_ABS "${_src}")
	else ()
		list(APPEND _SIM_SOURCES_ABS "${CMAKE_SOURCE_DIR}/${_src}")
	endif ()
endforeach ()

# Ensure the headless windowing/sound backends are present (a graphical
# backend left them out; backend=none already has them — guard against
# duplicates).
foreach (_headless
		src/simutrans/sys/simsys_posix.cc
		src/simutrans/sys/clipboard_internal.cc
		src/simutrans/display/simgraph0.cc
		src/simutrans/sound/no_sound.cc)
	set(_abs "${CMAKE_SOURCE_DIR}/${_headless}")
	get_filename_component(_base "${_headless}" NAME)
	# simgraph0 and simgraph16 both define the renderer symbols; only add
	# simgraph0 when the graphical simgraph16 isn't already in the list.
	if (_base STREQUAL "simgraph0.cc")
		set(_has_gfx16 FALSE)
		foreach (_s IN LISTS _SIM_SOURCES_ABS)
			if (_s MATCHES "simgraph16\\.cc$")
				set(_has_gfx16 TRUE)
			endif ()
		endforeach ()
		if (_has_gfx16)
			continue ()
		endif ()
	endif ()
	if (NOT _abs IN_LIST _SIM_SOURCES_ABS)
		list(APPEND _SIM_SOURCES_ABS "${_abs}")
	endif ()
endforeach ()

add_executable(bench_pak
	bench_pak.cc
)

target_sources(bench_pak PRIVATE ${_SIM_SOURCES_ABS})
target_include_directories(bench_pak PRIVATE ${_SIM_INCLUDES})
# SIMSYS_POSIX_ALLOW_COLOUR_DEPTH lets the headless posix sys compile under
# COLOUR_DEPTH=16 (it never draws — see the guard in simsys_posix.cc).
target_compile_definitions(bench_pak PRIVATE ${_SIM_DEFS} SIMUTRANS_NO_MAIN=1 SIMSYS_POSIX_ALLOW_COLOUR_DEPTH=1)
target_compile_options(bench_pak PRIVATE ${_SIM_OPTS})
target_link_libraries(bench_pak PRIVATE ${_SIM_LIBS})

# Match simutrans's revision header generator.
if (TARGET svnheader)
	add_dependencies(bench_pak svnheader)
endif ()
