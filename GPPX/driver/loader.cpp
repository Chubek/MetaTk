#include "Driver.hpp"
#include <algorithm>
#include <set>
#include <vector>

namespace gppx::driver {
namespace fs=std::filesystem;
namespace {
bool isPlugin(const fs::path& p) {
    auto ext=p.extension().string();
    return ext==".so" || ext==".dylib" || ext==".dll";
}
fs::path normalized(const fs::path& p) {
    std::error_code ec;
    auto path=fs::weakly_canonical(p,ec);
    if (!ec) return path;
    path=fs::absolute(p,ec);
    return ec?p:path.lexically_normal();
}
}
/** Load explicitly named shared libraries or all shared libraries directly in
 * given directories. Directory traversal is nonrecursive, deterministic,
 * deduplicated within this invocation, and does not follow directory symlinks.
 * NOTE: shared libraries execute arbitrary native code; only trust your plugins.
 */
void plugins(Engine& engine,const std::vector<fs::path>& paths) {
    std::set<fs::path> seen;
    std::vector<fs::path> loads;
    for (const auto& input:paths) {
        if (input.empty()) throw Error("plugin path is empty");
        std::error_code ec;
        auto canonical=normalized(input);
        if (fs::is_directory(canonical,ec)) {
            std::vector<fs::path> members;
            fs::directory_iterator it(canonical,fs::directory_options::skip_permission_denied,ec);
            if (ec) throw Error("cannot enumerate plugin directory: "+canonical.string()+": "+ec.message());
            for (const auto& entry:it) {
                if (entry.is_regular_file(ec) && isPlugin(entry.path())) members.push_back(normalized(entry.path()));
                if (ec) throw Error("cannot inspect plugin in "+canonical.string()+": "+ec.message());
            }
            std::sort(members.begin(),members.end());
            for (const auto& member:members) if (seen.insert(member).second) loads.push_back(member);
        } else {
            if (ec || !fs::is_regular_file(canonical)) throw Error("plugin path is not a regular file: "+input.string());
            if (!isPlugin(canonical)) throw Error("plugin has no shared-library suffix: "+canonical.string());
            if (seen.insert(canonical).second) loads.push_back(canonical);
        }
    }
    for (const auto& file:loads) {
        try {engine.loadPlugin(file);}
        catch (const std::exception& ex) {throw Error("loading "+file.string()+": "+ex.what());}
    }
}
}
