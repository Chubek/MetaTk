/**
 * metalib/d/Loader.d — MetaML host module `Loader` (D dialect): dynamic loading and module search paths.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Loader.h):
 *   Loader.load : String -> Result Unit String
 *   Loader.resolve : String -> Option String
 *   Loader.search_path : Unit -> [String]
 *   Loader.add_path : String -> Unit
 *   Loader.stdlib : Unit -> Option String
 *   Loader.loaded : Unit -> [String]
 */
module metalib.d.Loader;

import MetaML;
import metalib.d.Support;
import metalib.d.Frontend;

void installLoader(Session s)
{
    prim(s, "Loader.load", "String -> Result Unit String", 1, delegate Value(C c, V a) {
        auto r = c.session.load_file(S(a[0]));
        if (r.is_err()) return err(first_diag(r.unwrap_err()));
        return ok(unit());
    });
    prim(s, "Loader.resolve", "String -> Option String", 1, delegate Value(C c, V a) {
        auto r = c.session.resolve_import(S(a[0]));
        if (r.isNull) return none();
        return some(str(r.get));
    });
    prim(s, "Loader.search_path", "Unit -> [String]", 1, delegate Value(C c, V a) => strings(c.session.search_paths()));
    prim(s, "Loader.add_path", "String -> Unit", 1, delegate Value(C c, V a) {
        c.session.add_search_path(S(a[0]));
        return unit();
    });
    prim(s, "Loader.stdlib", "Unit -> Option String", 1, delegate Value(C c, V a) {
        if (!c.session.stdlib_dir().length) return none();
        return some(str(c.session.stdlib_dir()));
    });
    prim(s, "Loader.loaded", "Unit -> [String]", 1, delegate Value(C c, V a) => strings(c.session.imported_files()));
}

