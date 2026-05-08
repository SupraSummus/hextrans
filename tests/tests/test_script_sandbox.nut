//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests guarding the Squirrel script sandbox.
//


function test_script_sandbox_no_shell_exec()
{
	local root = ::getroottable()
	foreach (name in ["system", "getenv", "remove", "rename"]) {
		if (name in root) {
			throw "Squirrel sandbox bypass: ::" + name + "() is exposed to scripts"
		}
	}
}
