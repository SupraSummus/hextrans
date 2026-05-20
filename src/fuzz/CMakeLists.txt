#
# This file is part of the Simutrans project under the artistic licence.
# (see licence.txt)
#

# Coverage-guided fuzz harnesses for the network wire parser.
#
# Opt in with -DSIMUTRANS_BUILD_FUZZERS=ON on a clang configure, and
# enable the sanitizer flags the fuzzer link expects (-fsanitize=fuzzer
# plus address/undefined).  The target reuses the NETTOOL source subset:
# parser-only, no karte_t/game-state dependencies.

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
