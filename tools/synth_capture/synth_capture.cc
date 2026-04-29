// Standalone capture tool: dump the engine's synth ground rasteriser
// output as PPM files for use as ground truth on the pakset side.
//
// The pakset's parametric pipeline (hextrans-pak128, see its
// CLAUDE.md) needs PNG references for every (slope, climate)
// combination so an external renderer can be diffed against the
// engine's authoritative output.  This tool produces the references
// directly from the engine's rasterisation code without going through
// image_t, simgraph or a running simutrans process — synth_ground_raster.h
// is header-only by design, and the partition solver is too.
//
// Output: PPM (binary, P6, 8-bit RGB) under <outdir>/ground/, one file
// per (slope, climate) named `slope_<id>_climate_<id>.ppm`.  The
// transparent background is rendered as solid black (0,0,0) — it never
// overlaps with any climate base colour (smallest channel value at
// climate 1 = tropic = 6/31 = 49/255).
//
//   ./synth_capture out/      # dumps references into ./out/ground/
//
// Usage from the pakset repo: copy `out/ground/*.ppm` into
// `models/parametric/ground/refs/` and convert to PNG if desired
// (PPM is enough for the diff tool).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "simutrans/descriptor/synth_ground_raster.h"
#include "simutrans/descriptor/synth_geometry.h"
#include "simutrans/descriptor/synth_plane_partition.h"
#include "simutrans/dataobj/ribi.h"


// Use a representative raster width.  pak64 ships at W=64; the unit
// `u = W/4 = 16` divides cleanly and matches the existing tooling
// (hex_proj_test uses the same).  Any future scaling sweep would
// parameterise this; for the smoke test, one width is enough.
static constexpr sint32 W = 64;
static constexpr sint32 U = W / 4;

// `TILE_HEIGHT_STEP` is normally `env_t::pak_tile_height_step`, set at
// runtime from the loaded pakset (16 for pak64 / pak128).  This tool
// runs standalone with no env_t initialised, so pin the same default
// `hex_proj_test.cc` uses.
static constexpr sint16 HEIGHT_STEP = 16;


// Convert RGB555 PIXVAL to 8-bit RGB.  Channels live in 0..31 and are
// scaled by 255/31 so flat saturated colours land at exactly 0xFF.
static void rgb555_to_rgb8(PIXVAL p, unsigned char rgb[3])
{
	const unsigned r5 = (p >> 10) & 0x1F;
	const unsigned g5 = (p >>  5) & 0x1F;
	const unsigned b5 =  p        & 0x1F;
	rgb[0] = (unsigned char)((r5 * 255 + 15) / 31);
	rgb[1] = (unsigned char)((g5 * 255 + 15) / 31);
	rgb[2] = (unsigned char)((b5 * 255 + 15) / 31);
}


static bool write_ppm(const std::string &path, const PIXVAL *buf, sint32 w, sint32 h)
{
	FILE *f = fopen(path.c_str(), "wb");
	if (!f) {
		fprintf(stderr, "synth_capture: cannot open %s for writing\n", path.c_str());
		return false;
	}
	fprintf(f, "P6\n%d %d\n255\n", (int)w, (int)h);
	for (sint32 y = 0; y < h; y++) {
		for (sint32 x = 0; x < w; x++) {
			unsigned char rgb[3];
			rgb555_to_rgb8(buf[y * w + x], rgb);
			fwrite(rgb, 1, 3, f);
		}
	}
	fclose(f);
	return true;
}


// Write a small manifest describing the bbox / image_y of the captured
// references, so the pakset-side renderer can verify it is producing
// the same canvas size and offset.  One JSON-ish line per entry; kept
// dependency-free so the tool stays single-TU.
static FILE *open_manifest(const std::string &outdir)
{
	const std::string path = outdir + "/manifest.txt";
	FILE *f = fopen(path.c_str(), "wb");
	if (!f) {
		fprintf(stderr, "synth_capture: cannot open %s for writing\n", path.c_str());
	}
	return f;
}


int main(int argc, char **argv)
{
	const std::string outdir = (argc > 1) ? argv[1] : ".";
	const std::string ground_dir = outdir + "/ground";
	{
		// mkdir -p, ignoring failures (the parent might already exist).
		const std::string cmd = "mkdir -p '" + ground_dir + "'";
		if (system(cmd.c_str()) != 0) {
			// mkdir -p never errors when the dir exists; if it really
			// failed, the fopen below will surface the error per-file.
		}
	}

	const synth_overlay::synth_hex_geometry_t geom =
		synth_overlay::synth_hex_geometry(U, HEIGHT_STEP);

	FILE *manifest = open_manifest(outdir);
	if (manifest) {
		fprintf(manifest, "# synth ground reference manifest\n");
		fprintf(manifest, "# columns: slope climate w h image_y\n");
		fprintf(manifest, "# u=%d height_step=%d lift=%d top_pad=%d\n",
		        (int)U, (int)HEIGHT_STEP, (int)geom.lift, (int)geom.top_pad);
	}

	int written = 0;
	int skipped = 0;
	for (int s = 0; s < slope_t::max_slopes; s++) {
		// Match `synth_overlay::init`'s validity gate so we only emit
		// references for slopes that can appear on real terrain (per-edge
		// height delta ≤ 1).
		uint8 ch[hex_corner_t::count];
		synth_overlay::decode_corner_heights((slope_t::type)s, ch);
		bool valid = true;
		for (int i = 0; i < hex_corner_t::count; i++) {
			const int j = (i + 1) % hex_corner_t::count;
			if (ch[i] > ch[j] + 1 || ch[j] > ch[i] + 1) {
				valid = false;
				break;
			}
		}
		if (!valid) {
			skipped++;
			continue;
		}

		synth_overlay::plane_partition::hex_partition_t partition;
		partition.region_count = 0;
		if (!synth_overlay::plane_partition::find_min_partition(ch, partition)) {
			fprintf(stderr, "synth_capture: no partition for slope %d\n", s);
			return 2;
		}

		for (uint8 c = 0; c < synth_overlay::ground_climate_slots; c++) {
			PIXVAL *buf = new PIXVAL[geom.w * geom.h];
			memset(buf, 0, sizeof(PIXVAL) * geom.w * geom.h);
			synth_overlay::rasterise_ground(buf, geom, (slope_t::type)s, c, partition);

			char name[64];
			snprintf(name, sizeof(name), "slope_%03d_climate_%d.ppm", s, (int)c);
			const std::string path = ground_dir + "/" + name;
			const bool ok = write_ppm(path, buf, geom.w, geom.h);
			delete[] buf;
			if (!ok) {
				if (manifest) fclose(manifest);
				return 1;
			}
			if (manifest) {
				fprintf(manifest, "%d %d %d %d %d\n", s, (int)c, (int)geom.w, (int)geom.h,
				        (int)geom.image_y());
			}
			written++;
		}
	}

	if (manifest) fclose(manifest);
	fprintf(stderr, "synth_capture: wrote %d references (%d invalid slopes skipped)\n",
	        written, skipped);
	return 0;
}
