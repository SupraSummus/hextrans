this.hm_commands <- [] // array to store the commands
this.player <- null

// Slope constants of the toolkit.  They are taken from the engine's `slope`
// table (script_base.nut, which mirrors slope_t in dataobj/ribi.h) instead of
// being spelled out, so they cannot drift from the slope encoding again.
// The hex grid has no due-east / due-west edge: E* and W* are the legacy
// square diagonals (`slope.east` / `slope.west`) and their doubled heights.
class hm_slope {
  FL = slope.flat
  N1 = slope.north_narrow
  N2 = slope.north_double
  S1 = slope.south_narrow
  S2 = slope.south_double
  E1 = slope.east
  E2 = slope.east * 2
  W1 = slope.west
  W2 = slope.west * 2
  UP = slope.all_up_slope
  DOWN = slope.all_down_slope
}

class hm_base_tl {
  function exec(player, origin) {
    return null
  }
}
