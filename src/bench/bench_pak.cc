/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/*
 * Minimal perf-test rig for pakset *loading*.
 *
 * Drives the same descriptor-read path the in-game loader uses
 * (`pakset_manager_t::load_pak_from_fp`, the entry after `dr_fopen`),
 * once per `.pak` file under each directory given on the command line,
 * and reports wall time over a few iterations.  Like `fuzz_pak` it needs
 * only `dbg` plus the null renderer and the throwing fatal hook — no
 * `karte_t`, no env/translator/pakset setup — because the read phase is
 * self-contained.
 *
 * Each timed iteration runs in a forked child.  Reading a pak registers
 * descriptors into the readers' own static tables (hausbauer_t,
 * vehiclebuilder, ...), and there is no clean in-process reset for those
 * — freeing the descriptor DAG would leave the tables dangling and the
 * next load walks freed memory.  Fork-after-preload sidesteps it: every
 * child loads exactly once into pristine tables and exits, while the
 * preloaded file bytes are shared copy-on-write so re-reading the disk
 * isn't repaid per iteration.
 *
 * Scope: this measures the per-file decode + node-tree build + image
 * registration phase, which is the part that scales with tile size /
 * pakset size.  It deliberately does NOT run `finish_loading`
 * (xref resolution + checksum); that phase is comparatively cheap, is
 * private to pakset_manager, and needs a complete, consistent pakset to
 * succeed.  File bytes are slurped into memory once up front, so the
 * timed loop measures parsing rather than disk I/O.
 *
 * Build: configure with -DSIMUTRANS_BACKEND=none -DSIMUTRANS_BUILD_BENCH=ON
 * and build the `bench_pak` target.  `tools/bench_pak.py` does the
 * configure + build + fetch-paksets + run end to end.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>

#include "../simutrans/dataobj/pakset_manager.h"
#include "../simutrans/display/simgraph.h"
#include "../simutrans/simdebug.h"
#include "../simutrans/utils/log.h"
#include "../simutrans/utils/searchfolder.h"


namespace { struct bench_fatal {}; }


// Defensive dbg->fatal inside the readers means "this file is malformed",
// not "abort the benchmark".  Route it through a throw that unwinds back to
// the per-file recovery point (same idiom as fuzz_pak), so one bad pak is
// counted as a rejection instead of killing the run.
static void bench_fatal_hook(const char * /*buffer*/)
{
	throw bench_fatal{};
}


namespace {

// One preloaded pak file: its bytes, kept resident so the timed loop never
// touches the disk.
struct pak_blob_t {
	std::string         name;
	std::vector<uint8_t> bytes;
};

struct pakset_t {
	std::string             path;
	std::vector<pak_blob_t> blobs;
	size_t                  total_bytes = 0;
};


// Recursively gather every *.pak under `path` and slurp it into memory.
// Uses the engine's own searchfolder_t so the file set matches what the
// real loader would walk (same recursion depth as load_paks_from_directory).
static bool collect_pakset(const std::string &path, pakset_t &out)
{
	out.path = path;

	// searchfolder_t treats a non-slash-terminated path's last component as a
	// filename pattern (so "foo/pak" looks for "pak.pak" in foo/).  The real
	// loader's env_t::pak_dir always ends with '/'; mirror that here.
	std::string dir = path;
	if (!dir.empty() && dir.back() != '/') {
		dir += '/';
	}

	searchfolder_t find;
	const int max = find.search(dir, "pak", searchfolder_t::SF_PREPEND_PATH, 4);
	if (max <= 0) {
		return false;
	}

	for (char *const &pak_filename : find) {
		FILE *fp = fopen(pak_filename, "rb");
		if (fp == NULL) {
			continue;
		}
		fseek(fp, 0, SEEK_END);
		const long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if (len < 0) {
			fclose(fp);
			continue;
		}

		pak_blob_t blob;
		blob.name = pak_filename;
		blob.bytes.resize((size_t)len);
		const size_t got = fread(blob.bytes.data(), 1, (size_t)len, fp);
		fclose(fp);
		blob.bytes.resize(got);

		out.total_bytes += blob.bytes.size();
		out.blobs.push_back(std::move(blob));
	}

	return !out.blobs.empty();
}


// Result a child reports back over the pipe.
struct iter_result_t {
	double ms;
	size_t rejects;
};


// Parse every preloaded blob once, in this (child) process.  `rejects`
// counts files the reader refused (return false or fatal-hook throw).
static double parse_once(const pakset_t &pak, size_t &rejects)
{
	rejects = 0;
	const auto t0 = std::chrono::steady_clock::now();

	for (const pak_blob_t &blob : pak.blobs) {
		FILE *fp = fmemopen(const_cast<uint8_t *>(blob.bytes.data()), blob.bytes.size(), "rb");
		if (fp == NULL) {
			rejects++;
			continue;
		}
		try {
			if (!pakset_manager_t::load_pak_from_fp(fp, blob.name.c_str())) {
				rejects++;
			}
		}
		catch (const bench_fatal &) {
			rejects++;
		}
		fclose(fp);
	}

	const auto t1 = std::chrono::steady_clock::now();
	return std::chrono::duration<double, std::milli>(t1 - t0).count();
}


// Run one timed iteration in a forked child, so the read populates fresh
// reader tables that die with the child (see the file header for why an
// in-process reload isn't safe).  Returns false if the child died without
// reporting (e.g. crashed on malformed input).
static bool run_iteration(const pakset_t &pak, iter_result_t &out)
{
	int fds[2];
	if (pipe(fds) != 0) {
		return false;
	}

	const pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return false;
	}

	if (pid == 0) {
		// Child: time the parse, report, and skip global destructors (the
		// engine globals are only partially initialised here).
		close(fds[0]);
		iter_result_t r;
		r.ms = parse_once(pak, r.rejects);
		const ssize_t w = write(fds[1], &r, sizeof(r));
		(void)w;
		close(fds[1]);
		_exit(0);
	}

	// Parent: collect the result and reap the child.
	close(fds[1]);
	const ssize_t got = read(fds[0], &out, sizeof(out));
	close(fds[0]);

	int status = 0;
	waitpid(pid, &status, 0);

	return got == (ssize_t)sizeof(out);
}


static void report(const pakset_t &pak, const std::vector<double> &times, size_t rejects, bool json)
{
	std::vector<double> sorted = times;
	std::sort(sorted.begin(), sorted.end());
	const double min = sorted.front();
	const double median = sorted[sorted.size() / 2];
	double sum = 0.0;
	for (double t : sorted) {
		sum += t;
	}
	const double mean = sum / sorted.size();

	const double mib = (double)pak.total_bytes / (1024.0 * 1024.0);
	// Throughput is reported off the best (min) time — the least
	// noisy estimate of the steady-state parse rate.
	const double mib_per_s = min > 0.0 ? mib / (min / 1000.0) : 0.0;

	if (json) {
		printf("{\"pakset\":\"%s\",\"files\":%zu,\"bytes\":%zu,\"rejects\":%zu,"
		       "\"iterations\":%zu,\"min_ms\":%.3f,\"median_ms\":%.3f,"
		       "\"mean_ms\":%.3f,\"mib_per_s\":%.1f}\n",
		       pak.path.c_str(), pak.blobs.size(), pak.total_bytes, rejects,
		       times.size(), min, median, mean, mib_per_s);
	}
	else {
		printf("pakset: %s\n", pak.path.c_str());
		printf("  files:       %zu\n", pak.blobs.size());
		printf("  bytes:       %.1f MiB\n", mib);
		printf("  rejects:     %zu\n", rejects);
		printf("  load(parse): min %.1f ms  median %.1f ms  mean %.1f ms  (N=%zu)\n",
		       min, median, mean, times.size());
		printf("  throughput:  %.0f MiB/s\n", mib_per_s);
		printf("\n");
	}
}

} // namespace


static void usage(const char *argv0)
{
	fprintf(stderr,
		"usage: %s [--iterations N] [--json] <pakset-dir> [<pakset-dir> ...]\n"
		"\n"
		"Times the descriptor-read phase of loading each pakset directory.\n"
		"  --iterations N  parse passes per pakset (default 5)\n"
		"  --json          one JSON object per pakset instead of a table\n",
		argv0);
}


int main(int argc, char **argv)
{
	dbg = new log_t(NULL, false, false, false, NULL, NULL);
	// image_reader_t calls gfx->register_image() per IMG node.  Under a
	// colour build (COLOUR_DEPTH != 0, the representative consumer config)
	// bind the real simgraph16 software renderer so register_image runs its
	// player-colour scan and array growth — the load-time image work a
	// client does; the heavy rezoom/recode is deferred to draw and isn't
	// reached.  A headless COLOUR_DEPTH=0 build has only the null renderer.
#if COLOUR_DEPTH != 0
	gfx = simgraph_select(SIMGRAPH_TYPE_SOFTWARE);
#else
	gfx = simgraph_select(SIMGRAPH_TYPE_NULL);
#endif
	log_t::set_fatal_hook(bench_fatal_hook);

	int iterations = 5;
	bool json = false;
	std::vector<std::string> dirs;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp(a, "--iterations") == 0 && i + 1 < argc) {
			iterations = atoi(argv[++i]);
			if (iterations < 1) {
				iterations = 1;
			}
		}
		else if (strcmp(a, "--json") == 0) {
			json = true;
		}
		else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
			usage(argv[0]);
			return 0;
		}
		else {
			dirs.push_back(a);
		}
	}

	if (dirs.empty()) {
		usage(argv[0]);
		return 2;
	}

	int exit_code = 0;
	for (const std::string &dir : dirs) {
		pakset_t pak;
		if (!collect_pakset(dir, pak)) {
			fprintf(stderr, "bench_pak: no .pak files found under '%s'\n", dir.c_str());
			exit_code = 1;
			continue;
		}

		std::vector<double> times;
		times.reserve(iterations);
		size_t rejects = 0;
		for (int it = 0; it < iterations; it++) {
			iter_result_t r;
			if (!run_iteration(pak, r)) {
				fprintf(stderr, "bench_pak: iteration %d on '%s' did not complete\n",
				        it, dir.c_str());
				exit_code = 1;
				continue;
			}
			// The reject count is stable across iterations (identical
			// input); keep the latest for the report.
			rejects = r.rejects;
			times.push_back(r.ms);
		}

		if (times.empty()) {
			continue;
		}
		report(pak, times, rejects, json);
	}

	// We deliberately initialise only the slice of engine globals the read
	// path needs (dbg + null gfx); running the C++ static destructors over
	// the rest at normal return crashes on the partially-set-up state.  The
	// benchmark is done — flush and skip global teardown via _exit, the same
	// move the headless fuzz harnesses rely on libFuzzer's _Exit for.
	fflush(stdout);
	fflush(stderr);
	_exit(exit_code);
}
