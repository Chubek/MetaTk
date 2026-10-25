#ifndef METAIL_KERNEL_REGISTRY_HPP
#define METAIL_KERNEL_REGISTRY_HPP
#include "MetaIL.hpp"
#include <algorithm>
#include <filesystem>
#include <locale>
#include <vector>

namespace metail {
struct KernelQuery {
    std::string keywords;           // All whitespace-separated terms must match.
    std::vector<std::string> tags;   // All tags must match exactly.
    std::string use;                // All terms must match the use-case text.
    std::string kind;               // Empty, "analysis", "rewrite", or "builtin".
};
struct Kernel {
    std::string name;
    std::filesystem::path path;
    std::string kind, implementation, description;
    std::vector<std::string> keywords, tags, uses;

    // Read on demand; throws if the file has disappeared or is unreadable.
    // Returns current bytes on disk, not an archived snapshot of the declaration.
    std::string source() const { return Context::read_file(path.string()); }
};
struct KernelFile {
    Kernel kernel;
    std::string contents;
};

// A value-owned, name-sorted snapshot of the runtime's validated declarations.
// It outlives its originating Context. Const discovery methods may run in parallel.
// Search is case-insensitive ASCII, literal (no regex), and combines filters with
// AND. Empty queries return every kernel. No match returns an empty vector.
class KernelRegistry {
    std::vector<Kernel> kernels_;
    static std::string lower(std::string text) {
        for (char &c:text) if (c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');
        return text;
    }
    static std::vector<std::string> lines(const char *text) {
        std::vector<std::string> result;
        std::istringstream stream(text);
        for (std::string line;std::getline(stream,line);) result.push_back(std::move(line));
        return result;
    }
    static std::string joined(const std::vector<std::string> &values) {
        std::string out;
        for (const auto &value:values) { out+=value;out+=' '; }
        return out;
    }
    static bool contains_all(const std::string &text, const std::string &query) {
        const auto haystack=lower(text);
        std::istringstream terms(lower(query));
        // The classic locale makes tokenization independent of application locale.
        terms.imbue(std::locale::classic());
        for (std::string term;terms>>term;)
            if (haystack.find(term)==std::string::npos) return false;
        return true;
    }
    static bool matches(const Kernel &k, const KernelQuery &q) {
        if (!q.kind.empty() && lower(q.kind)!=k.kind) return false;
        for (const auto &tag:q.tags) {
            if (std::none_of(k.tags.begin(),k.tags.end(),[&](const std::string &t) {
                return lower(t)==lower(tag);
            })) return false;
        }
        return contains_all(k.name+" "+k.description+" "+k.implementation+" "+joined(k.keywords)+
                            joined(k.tags)+joined(k.uses),q.keywords) &&
               contains_all(joined(k.uses),q.use);
    }
public:
    explicit KernelRegistry(Context &context) { refresh(context); }
    explicit KernelRegistry(const std::string &home={}) {
        Context context(home);refresh(context);
    }
    // Strong exception guarantee: a failed refresh retains the previous snapshot.
    void refresh(Context &context) {
        std::vector<Kernel> next;
        size_t count=0;
        auto *c=context.native_handle();
        if (metail_kernel_count(c,&count)!=METAIL_OK) throw Error(metail_last_error(c));
        next.reserve(count);
        for (size_t i=0;i<count;++i) {
            metail_kernel_info info{};
            if (metail_kernel_get(c,i,&info)!=METAIL_OK) throw Error(metail_last_error(c));
            next.push_back({info.name,std::filesystem::absolute(info.path).lexically_normal(),
                info.kind,info.implementation,info.description,lines(info.keywords),lines(info.tags),lines(info.uses)});
        }
        std::sort(next.begin(),next.end(),[](const Kernel &a,const Kernel &b) { return a.name<b.name; });
        kernels_.swap(next);
    }
    const std::vector<Kernel> &entries() const noexcept { return kernels_; }
    std::size_t size() const noexcept { return kernels_.size(); }
    // Exact, case-sensitive identifier lookup. Pointer valid until refresh/destruction.
    const Kernel *find(const std::string &name) const noexcept {
        auto i=std::lower_bound(kernels_.begin(),kernels_.end(),name,
            [](const Kernel &k,const std::string &n) { return k.name<n; });
        return i!=kernels_.end() && i->name==name ? &*i : nullptr;
    }
    std::vector<Kernel> search(const KernelQuery &query={}) const {
        std::vector<Kernel> result;
        for (const auto &k:kernels_) if (matches(k,query)) result.push_back(k);
        return result;
    }
    std::vector<std::filesystem::path> paths(const KernelQuery &query={}) const {
        std::vector<std::filesystem::path> result;
        for (const auto &k:kernels_) if (matches(k,query)) result.push_back(k.path);
        return result;
    }
    KernelFile request(const std::string &name) const {
        const auto *k=find(name);
        if (!k) throw Error("unknown kernel "+name);
        return {*k,k->source()};
    }
    std::vector<KernelFile> request(const KernelQuery &query) const {
        std::vector<KernelFile> result;
        for (const auto &k:kernels_) if (matches(k,query)) result.push_back({k,k.source()});
        return result;
    }
    // Convert discovery results to an explicit API/CLI pass list. An empty list
    // is an error so a failed search can never accidentally select all passes.
    static std::string passes(const std::vector<Kernel> &kernels) {
        if (kernels.empty()) throw Error("cannot build a pass list from an empty selection");
        std::string result;
        const bool analysis=kernels.front().kind=="analysis";
        for (const auto &k:kernels) {
            if ((k.kind=="analysis")!=analysis) throw Error("cannot mix analysis and transformation passes");
            if (!result.empty()) result+=',';
            result+=k.name;
        }
        return result;
    }
};
} // namespace metail
#endif
