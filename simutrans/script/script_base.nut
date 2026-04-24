/**
 * Base file for scripting
 */

// table to hold routines for debug
debug <- {}

function min(a, b) { return a < b ? a : b }
function max(a, b) { return a > b ? a : b }

/**
 * load / save support
 * the persistent table will be written / restored during save / load
 * only plain data is saved: no classes / instances / functions, no cyclic references
 */
persistent <- {}

/**
 * writes the persistent table to a string
 */
function save()
{
	if (typeof(persistent) != "table") {
		throw("error during saving: the variable persistent is not a table")
	}
	local str = "persistent = " + recursive_save(persistent, "\t", [ persistent ] )

	return str
}

function is_identifier(str)
{
	return (str.toalnum() == str)  &&  (str[0] < '0'  ||  str[0] > '9')
}

function recursive_save(table, indent, table_stack)
{
	local isarray = typeof(table) == "array"
	local str = (isarray ? "[" : "{") + "\n"
	foreach(key, val in table) {
		str += indent
		if (!isarray) {
			if (typeof(key)=="string") {
				if (is_identifier(key)) {
					str += key + " = "
				}
				else {
					str += "[\"" + key + "\"] = "
				}
			}
			else {
				str += "[" + key + "] = "
			}
		}
		while( typeof(val) == "weakref" )
			val = val.ref

		switch( typeof(val) ) {
			case "null":
				str += "null"
				break
			case "integer":
			case "float":
			case "bool":
				str += val
				break
			case "string":
				str += "@\"" + val + "\""
				break
			case "array":
			case "table":
				if (!table_stack.find(val)) {
						table_stack.push( table )
						str += recursive_save(val, indent + "\t", table_stack )
						table_stack.pop()
				}
				else {
					// cyclic reference - good luck with resolving
					str += "null"
				}
				break
			case "generator":
				str += "null"
				break
			case "instance":
			default:
				if ("_save" in val) {
					str += val._save()
					break
				}
				str += "\"unknown(" + typeof(val) + ")\""
		}
		if (str.slice(-1) != "\n") {
			str += ",\n"
		}
		else {
			str = str.slice(0,-1) + ",\n"
		}

	}
	str += indent.slice(0,-1) + (isarray ? "]" : "}") + "\n"
	return str
}


/////////////////////////////////////
/**
 * translatable texts
 */
class ttext {
	text_raw = "" // raw text
	text_tra = "" // translated text
	// tables with information to replace variables {key} by their values
	var_strings = null
	var_values  = null
	// these cannot be initialized here to {}, otherwise
	// all instances will refer to the same table!

	constructor(k) {
		if (type(k) != "string") {
			throw("ttext must be initialized with string")
		}
		text_raw = k
		text_tra = k
		var_strings = []
		var_values  = {}
		find_variables(text_raw)
	}

	function get_translated_text()
	{
		return translate(text_raw)
	}

	function find_variables(k) {
		var_strings = []
		// find variable tags
		local reg = regexp(@"\{([a-zA-Z_]+[a-zA-Z_0-9]*)\}");
		local start = 0;
		while (start < k.len()) {
			local res = reg.capture(k, start)
			if (res) {
				local vname = k.slice(res[1].begin, res[1].end)
				var_strings.append({ name = vname, begin = res[0].begin, end = res[0].end })
				start = res[0].end
			}
			else {
				start = k.len();
			}
		}
	}

	function _set(key, val)
	{
		var_values[key] <- val
	}

	function _tostring()
	{
		// get translated text
		local text_tra_ = get_translated_text()
		if (text_tra_ != text_tra) {
			text_tra = text_tra_
			// new text, search for identifiers
			find_variables( text_tra )
		}
		// create array of values, sort it per index in string
		local values = []
		foreach(val in var_strings) {
			local key = val.name
			if ( key in var_values ) {
				local valx = { begin = val.begin, end = val.end, value = var_values[key] }
				values.append(valx)
			}
		}
		values.sort( @(a,b) a.begin <=> b.begin )
		// create the resulting text piece by piece
		local res = ""
		local ind = 0
		foreach( e in values) {
			res += text_tra.slice(ind, e.begin)
			res += e.value
			ind = e.end
		}
		res += text_tra.slice(ind)
		return res
	}
}

/**
 * text from files
 */
class ttextfile extends ttext {

	file = null

	constructor(file_) {
		file = file_
		base.constructor("")
	}

	function get_translated_text()
	{
		return load_language_file(file)
	}


}

/////////////////////////////////////

function _extend_get(index) {
	if (index == "rawin"  ||  index == "rawget") {
		throw null // invoke default delegate
		return
	}
	local fname = "get_" + index
	if (fname in this) {
		local func = this[fname]
		if (typeof(func)=="function") {
			return func.call(this)
		}
	}
	throw null // invoke default delegate
}

/**
 * this class implements an extended get method:
 * everytime an index is not found it tries to call the method 'get_'+index
 */
class extend_get {

	_get = _extend_get

}

/////////////////////////////////////

class coord {
	x = -1
	y = -1

	constructor(_x, _y)  { x = _x; y = _y }
	function _add(other) { return coord(x + other.x, y + other.y) }
	function _sub(other) { return coord(x - other.x, y - other.y) }
	function _mul(fac)   { return coord(x * fac, y * fac) }
	function _div(fac)   { return coord(x / fac, y / fac) }
	function _unm()      { return coord(-x, -y) }
	function _typeof()   { return "coord" }
	function _tostring() { return coord_to_string(this) }
	function _save()     { return "coord(" + x + ", " + y + ")" }
	function href(text)  { return "<a href='(" + x + ", " + y + ")'>" + text + "</a>" }
}

class coord3d extends coord {
	z = -1

	constructor(_x, _y, _z)  { x = _x; y = _y; z = _z }
	function _add(other) { return coord3d(x + other.x, y + other.y, z + getz(other)) }
	function _sub(other) { return coord3d(x - other.x, y - other.y, z - getz(other)) }
	function _mul(fac)   { return coord3d(x * fac, y * fac, z * fac) }
	function _div(fac)   { return coord3d(x / fac, y / fac, z / fac) }
	function _unm()      { return coord3d(-x, -y, -z) }
	function _typeof()   { return "coord3d" }
	function _tostring() { return coord3d_to_string(this) }
	function _save()     { return "coord3d(" + x + ", " + y + ", " + z + ")" }
	function href(text)  { return "<a href='(" + x + ", " + y + ", " + z + ")'>" + text + "</a>" }

	function getz(other) { return ("z" in other) ? other.z : 0 }
}

/**
 * class that contains data to get access to an in-game factory
 */
class factory_x extends coord {

	_get = _extend_get
	function _tostring() { return "factory_x@" + coord_to_string(this) }

	/// input / output slots, will be filled by constructor
	input = {}
	output = {}

	// constructor is implemented in c++
}


/**
 * class that contains data to get access to an production slot of a factory
 */
class factory_production_x extends extend_get {
	/// coordinates of factory
	x = -1
	y = -1
	good = ""  /// name of the good to be consumed / produced
	index = -1 /// index to identify an io slot
	max_storage = 0  /// max storage of this slot
	scaling = 0

	constructor(x_, y_, n_, i_) {
		x = x_
		y = y_
		good  = n_
		index = i_
	}
}


/**
 * class to provide access to the game's list of all factories
 */
class factory_list_x {

	/// meta-method to be called in a foreach loop
	function _nexti(prev_index) {
	}

	/// meta method to retrieve factory by index in the global C++ array
	function _get(index) {
	}
}


/**
 * class that contains data to get access to an in-game player company
 */
class player_x extends extend_get {
	nr = 0 /// player number

	function _tostring() { return "player_x@" + nr }

	constructor(n_) {
		nr = n_
	}
}


/**
 * class that contains data to get access to an in-game halt
 */
class halt_x extends extend_get {
	id = 0 /// halthandle_t

	function _tostring() { return "halt_x@" + id }

	constructor(i_) {
		id = i_
	}
}


/**
 * class that contains data to get access to a line of convoys
 */
class line_x extends extend_get {
	id = 0 /// linehandle_t

	function _tostring() { return "line_x@" + id }

	constructor(i_) {
		id = i_
	}
}

/**
 * class to provide access to line lists
 */
class line_list_x {

	halt_id = 0
	player_id = 0
}

/**
 * class that contains data to get access to a tile (grund_t)
 */
class tile_x extends coord3d {

	_get = _extend_get
	function _tostring() { return "tile_x@" + coord3d_to_string(this) }

	function get_objects()
	{
		return tile_object_list_x(x,y,z)
	}
}

class tile_object_list_x {
	/// coordinates
	x = -1
	y = -1
	z = -1

	constructor(x_, y_, z_) {
		x = x_
		y = y_
		z = z_
	}
}

/**
 * class that contains data to get access to a grid square (planquadrat_t)
 */
class square_x extends coord {

	_get = _extend_get
	function _tostring() { return "square_x@" + coord_to_string(this) }
}


/**
 * class to provide access to convoy lists
 */
class convoy_list_x {

	use_world = 0
	halt_id = 0
	line_id = 0
}


/**
 * class that contains data to get access to an in-game convoy
 */
class convoy_x extends extend_get {
	id = 0 /// convoihandle_t

	function _tostring() { return "convoy_x@" + id }

	constructor(i_) {
		id = i_
	}
}


/**
 * class to provide access to the game's list of all cities
 */
class city_list_x {

	/// meta-method to be called in a foreach loop
	function _nexti(prev_index) {
	}

	/// meta method to retrieve city by index in the global C++ array
	function _get(index) {
	}
}


/**
 * class that contains data to get access to a city
 */
class city_x extends coord {

	_get = _extend_get
	function _tostring() { return "city_x@" + coord_to_string(this) }

}

/**
 * class to access in-game settings
 */
class settings {
}

/**
 * base class of map objects (obj_t)
 */
class map_object_x extends coord3d {

	_get = _extend_get
	function _tostring() { return "map_object_x@" + coord_to_string(this) }
}

class schedule_entry_x extends coord3d {

	function _tostring() { return "schedule_entry_x@" + coord3d_to_string(this) }

	/// load percentage
	load = 0
	/// waiting
	wait = 0
}

class dir {
	// Single-edge directions.  Bit layout matches ribi_t::_ribi in
	// src/simutrans/dataobj/ribi.h: the 6 bits run SE, S, SW, NW, N,
	// NE in koord::neighbours[] order.  Flat-top hex has no due-east
	// or due-west edge, so the old `east` / `west` constants are gone
	// — callers that want the old E/W should use southeast / northwest
	// (the closest hex edges in the current 2:1 isometric viewport,
	// see TODO.md → viewport port) or explicitly pick between the two
	// hex axes that used to be collapsed onto square E / W.
	static none      = 0
	static southeast = 1    // bit 0, neighbours[0]
	static south     = 2    // bit 1, neighbours[1]
	static southwest = 4    // bit 2, neighbours[2]
	static northwest = 8    // bit 3, neighbours[3]
	static north     = 16   // bit 4, neighbours[4]
	static northeast = 32   // bit 5, neighbours[5]
	static all       = 63

	// Straight axes: the 3 opposite-edge 2-bit combos.  Flat-top hex
	// has 3 axes (N-S, NE-SW, NW-SE) at 60° spacing; the old 4-bit
	// `eastwest` axis is gone.
	static northsouth          = 16 | 2    // N  | S  = 18
	static northeast_southwest = 32 | 4    // NE | SW = 36
	static northwest_southeast = 8  | 1    // NW | SE = 9

	// Neighbour-index lookup: nsew[i] = 1 << i, in koord::neighbours[]
	// order (SE, S, SW, NW, N, NE).  Kept under the old name for grep-
	// continuity; rename together with ribi_t::nesw once the square-
	// era callers are all gone.
	static nsew = [1, 2, 4, 8, 16, 32]
}

class slope {
	// Base-3 6-corner encoding, matches slope_t::type in
	// src/simutrans/dataobj/ribi.h.  Digit positions follow
	// hex_corner_t: E=1, SE=3, SW=9, W=27, NW=81, NE=243.
	static flat = 0
	static southeast = 3     ///< SE corner raised
	static southwest = 9     ///< SW corner raised
	static northwest = 81    ///< NW corner raised
	static northeast = 243   ///< NE corner raised
	static north = 3 + 9     ///< North slope: low edge N, S corners raised = 12
	static south = 81 + 243  ///< South slope: low edge S, N corners raised = 324
	static east  = 81 + 9    ///< East slope: 2 west corners raised = 90
	static west  = 243 + 3   ///< West slope: 2 east corners raised = 246
	// `raised` here is the single-height "all corners 1" value, used
	// by scripts as the iteration bound over "interesting" single-
	// height slopes — NOT the same as C++ `slope_t::raised`, which is
	// the bridgehead sentinel (= all_up_two = 728).
	static raised = 1 + 3 + 9 + 27 + 81 + 243  ///< all 6 corners at height 1 = 364
	static all_up_slope   = 801 ///< used for terraforming tools (matches ALL_UP_SLOPE in simconst.h, outside slope range)
	static all_down_slope = 802 ///< used for terraforming tools (matches ALL_DOWN_SLOPE in simconst.h, outside slope range)
}

class time_x {
	raw = 1
	year = 0
	month = 1
}

class time_ticks_x extends time_x {
	ticks = 0
	ticks_per_month = 0
	next_month_ticks = 0
}


/**
 * The same metamethod magic as in the class extend_get.
 */
table_with_extend_get <- {

	_get = _extend_get

}

/**
 * table to hold routines to access the world
 */
world <- {}
world.setdelegate(table_with_extend_get)


// table to hold routines for gui access
gui <- {}
