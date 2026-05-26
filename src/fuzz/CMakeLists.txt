#
# This file is part of the Simutrans project under the artistic licence.
# (see licence.txt)
#

# Coverage-guided fuzz harnesses.
#
# Opt in with -DSIMUTRANS_BUILD_FUZZERS=ON on a clang configure, and
# enable the sanitizer flags the fuzzer link expects (-fsanitize=fuzzer
# plus address/undefined).  fuzz_network reuses the NETTOOL source
# subset (parser-only, no karte_t/game-state dependencies).  fuzz_pak
# inherits the simutrans target's full source list — the descriptor
# readers self-register via static initialisers, so the type table is
# only populated when all of them link.

add_executable(fuzz_network
	fuzz_network.cc
)

target_compile_options(fuzz_network PRIVATE ${SIMUTRANS_COMMON_COMPILE_OPTIONS})
target_compile_definitions(fuzz_network PRIVATE NETTOOL=1 COLOUR_DEPTH=0)
target_compile_definitions(fuzz_network PRIVATE MSG_LEVEL=${SIMUTRANS_MSG_LEVEL})

if (SIMUTRANS_USE_SYSLOG)
	target_compile_definitions(fuzz_network PRIVATE SYSLOG=1)
endif ()

if (SIMUTRANS_USE_IP4_ONLY)
	target_compile_definitions(fuzz_network PRIVATE USE_IP4_ONLY=1)
endif ()

# Mirror nettool's source list — parser-only subset of simutrans,
# defined with NETTOOL=1.
target_sources(fuzz_network PRIVATE
	../simutrans/dataobj/freelist.cc
	../simutrans/network/memory_rw.cc
	../simutrans/network/network_address.cc
	../simutrans/network/network_cmd.cc
	../simutrans/network/network_packet.cc
	../simutrans/network/network_socket_list.cc
	../simutrans/simdebug.cc
	../simutrans/simmem.cc
	../simutrans/utils/cbuffer.cc
	../simutrans/utils/simstring.cc
	../simutrans/utils/fetchopt.cc
	../simutrans/utils/sha1.cc
	../simutrans/utils/sha1_hash.cc
	../simutrans/utils/log.cc
	../simutrans/network/network.cc
	../simutrans/network/network_file_transfer.cc
)

target_link_options(fuzz_network PRIVATE -fsanitize=fuzzer)
target_compile_options(fuzz_network PRIVATE -fsanitize=fuzzer-no-link)


#
# fuzz_pak: descriptor-tree reader, driven through the same FILE* path
# the in-game pakset loader uses (fmemopen wraps the fuzz bytes,
# `load_pak_from_fp` consumes them).  See fuzz_pak.cc.
#
# The harness needs the *real* registry of `obj_reader_t` subclasses —
# they self-register at static init via OBJ_READER_DEF — which means
# linking the full simutrans source set.  Rather than duplicate the
# list, we inherit it from the simutrans target (already populated by
# this point in configure), force the headless backend's compile
# defines, and skip simsys main() via SIMUTRANS_NO_MAIN so libFuzzer's
# own main wins at link time.

if (NOT SIMUTRANS_BACKEND STREQUAL "none")
	# fuzz_pak inherits the simutrans target's source set, which under
	# a graphical backend pulls in simsys_s2/simsys_w with their own
	# main() — SIMUTRANS_NO_MAIN only papers over simsys_posix.  Skip
	# fuzz_pak in that case; fuzz_network is unaffected.
	message(STATUS "fuzz_pak: skipped (requires SIMUTRANS_BACKEND=none)")
	return ()
endif ()

get_target_property(_SIM_SOURCES   simutrans SOURCES)
get_target_property(_SIM_INCLUDES  simutrans INCLUDE_DIRECTORIES)
get_target_property(_SIM_DEFS      simutrans COMPILE_DEFINITIONS)
get_target_property(_SIM_OPTS      simutrans COMPILE_OPTIONS)
get_target_property(_SIM_LIBS      simutrans LINK_LIBRARIES)

# Sources in SimutransSourceList.cmake are stored relative to the root
# CMakeLists (where they were added).  Re-rooting them to absolute
# paths keeps the build sane when target_sources is replayed from
# src/fuzz here.
set(_SIM_SOURCES_ABS "")
foreach (_src IN LISTS _SIM_SOURCES)
	# Headers carry HEADER_FILE_ONLY on the simutrans target (e.g.
	# the generated revision.h); inheriting that property cleanly is
	# fiddly, and they don't need to be in our source list anyway —
	# the corresponding .cc files reach them via the include path.
	if (_src MATCHES "\\.(h|hpp)$")
		continue ()
	endif ()
	if (IS_ABSOLUTE "${_src}")
		list(APPEND _SIM_SOURCES_ABS "${_src}")
	else ()
		list(APPEND _SIM_SOURCES_ABS "${CMAKE_SOURCE_DIR}/${_src}")
	endif ()
endforeach ()

add_executable(fuzz_pak
	fuzz_pak.cc
)

target_sources(fuzz_pak PRIVATE ${_SIM_SOURCES_ABS})
target_include_directories(fuzz_pak PRIVATE ${_SIM_INCLUDES})
target_compile_definitions(fuzz_pak PRIVATE ${_SIM_DEFS} SIMUTRANS_NO_MAIN=1)
target_compile_options(fuzz_pak PRIVATE ${_SIM_OPTS})
target_link_libraries(fuzz_pak PRIVATE ${_SIM_LIBS})

# Match simutrans's revision header generator.
if (TARGET svnheader)
	add_dependencies(fuzz_pak svnheader)
endif ()

target_link_options(fuzz_pak PRIVATE -fsanitize=fuzzer)
target_compile_options(fuzz_pak PRIVATE -fsanitize=fuzzer-no-link)
