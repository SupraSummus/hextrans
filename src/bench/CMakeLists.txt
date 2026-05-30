#
# This file is part of the Simutrans project under the artistic licence.
# (see licence.txt)
#

# bench_pak: minimal perf-test rig for pakset loading.  Drives the
# descriptor-read path (`load_pak_from_fp`) over every .pak in a
# directory and times it — see bench_pak.cc.
#
# Like fuzz_pak it needs the *real* registry of obj_reader_t subclasses
# (they self-register at static init via OBJ_READER_DEF), which means
# linking the full simutrans source set.  We inherit that set from the
# simutrans target rather than duplicate it, force the headless backend's
# defines, and skip simsys main() via SIMUTRANS_NO_MAIN (our own main
# wins).  Unlike the fuzz targets this is a plain executable: no
# libFuzzer, no sanitizer flags, builds under gcc or clang.  It forks per
# iteration rather than freeing descriptors in-process, so it needs no
# TRACK_DESCRIPTORS — the read populates fresh reader tables that die with
# the child.

if (NOT SIMUTRANS_BACKEND STREQUAL "none")
	# Inheriting the simutrans source set under a graphical backend pulls
	# in simsys_s2/simsys_w with their own main(); SIMUTRANS_NO_MAIN only
	# papers over simsys_posix.  Require the headless backend.
	message(STATUS "bench_pak: skipped (requires SIMUTRANS_BACKEND=none)")
	return ()
endif ()

get_target_property(_SIM_SOURCES   simutrans SOURCES)
get_target_property(_SIM_INCLUDES  simutrans INCLUDE_DIRECTORIES)
get_target_property(_SIM_DEFS      simutrans COMPILE_DEFINITIONS)
get_target_property(_SIM_OPTS      simutrans COMPILE_OPTIONS)
get_target_property(_SIM_LIBS      simutrans LINK_LIBRARIES)

# Sources are stored relative to the root CMakeLists; re-root to absolute
# paths so target_sources replays cleanly from src/bench (same as fuzz).
set(_SIM_SOURCES_ABS "")
foreach (_src IN LISTS _SIM_SOURCES)
	if (_src MATCHES "\\.(h|hpp)$")
		continue ()
	endif ()
	if (IS_ABSOLUTE "${_src}")
		list(APPEND _SIM_SOURCES_ABS "${_src}")
	else ()
		list(APPEND _SIM_SOURCES_ABS "${CMAKE_SOURCE_DIR}/${_src}")
	endif ()
endforeach ()

add_executable(bench_pak
	bench_pak.cc
)

target_sources(bench_pak PRIVATE ${_SIM_SOURCES_ABS})
target_include_directories(bench_pak PRIVATE ${_SIM_INCLUDES})
target_compile_definitions(bench_pak PRIVATE ${_SIM_DEFS} SIMUTRANS_NO_MAIN=1)
target_compile_options(bench_pak PRIVATE ${_SIM_OPTS})
target_link_libraries(bench_pak PRIVATE ${_SIM_LIBS})

# Match simutrans's revision header generator.
if (TARGET svnheader)
	add_dependencies(bench_pak svnheader)
endif ()
