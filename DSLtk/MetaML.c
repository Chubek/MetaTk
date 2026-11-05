// MetaML command line and replxx frontend. Compile this .c file as C++20.
#include "MetaML.hpp"
#include "metalib/cpp/Metalib.h"
#include <replxx.hxx>

#include <cerrno>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#if defined(_WIN32)
#include <io.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace
{
namespace fs = std::filesystem;
using metaml::RunReport;
using metaml::Session;
using Replxx = replxx::Replxx;

bool tty (int fd)
{
#if defined(_WIN32)
  return _isatty (fd);
#else
  return isatty (fd);
#endif
}

std::string trim (std::string_view s)
{
  auto b = s.find_first_not_of (" \t\r\n");
  return b == s.npos ? "" : std::string (s.substr (b, s.find_last_not_of (" \t\r\n") - b + 1));
}

std::string unquote (std::string s)
{
  s = trim (s);
  if (s.size () >= 2 && ((s.front () == '"' && s.back () == '"')
                        || (s.front () == '\'' && s.back () == '\'')))
    s = s.substr (1, s.size () - 2);
  if (s == "~" || s.starts_with ("~/"))
    if (const char *home = std::getenv ("HOME")) s.replace (0, 1, home);
  return s;
}

std::vector<std::string> env_paths (const char *name)
{
  std::vector<std::string> out;
  const char *env = std::getenv (name);
  if (!env || !*env) return out;
  std::string value (env);
  for (std::size_t b = 0;;)
    {
      auto e = value.find (':', b);
      auto part = value.substr (b, e == value.npos ? e : e - b);
      out.push_back (part.empty () ? "." : unquote (part));
      if (e == value.npos) break;
      b = e + 1;
    }
  return out;
}

std::string json_string (std::string_view s)
{
  static const char hex[] = "0123456789abcdef";
  std::string out = "\"";
  for (unsigned char c : s)
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
    else out += c;
  return out + '"';
}

struct Options
{
  struct Action { bool type; std::string source; };
  bool help = false, version = false, interactive = false, check = false, ast = false;
  bool trace = false, time = false, no_stdlib = false, no_history = false;
  bool no_init = false, quiet = false;
  std::string file, stdlib, history_file, color = "auto", diagnostic_format = "text";
  std::string engine = "xvm";
  std::vector<std::string> loads, paths, args;
  std::vector<Action> actions;
};

const char usage[] = R"(Usage: metaml [OPTIONS] [FILE] [-- PROGRAM_ARGS...]
  -h, --help                   Show this help
  -V, --version                Print the MetaML version
  -i, --interactive            Enter the REPL after supplied code
  -e, --eval EXPR              Evaluate EXPR (repeatable)
  -l, --load FILE              Preload FILE (repeatable)
  -c, --check                  Type-check supplied code without execution
      --type EXPR              Infer EXPR's type without execution (repeatable)
      --ast                    Print the AST without execution
      --trace                  Show evaluation calls
      --time                   Report elapsed execution time to stderr
  -I, --module-path DIR        Add a search directory (repeatable)
      --stdlib DIR             Select the standard-library directory
      --no-stdlib              Use only the built-in environment
      --engine NAME            xvm (default), portable, jit, tree, or qbe (opt-in)
      --history-file FILE      Select persistent REPL history
      --no-history             Disable persistent history
      --no-init                Skip ~/.metamlrc
      --color MODE             auto, always, or never
      --diagnostic-format FMT  text or json (one JSON object per diagnostic)
  -q, --quiet                  Suppress banner and automatic binding reports
      --                      Pass remaining arguments to Process.args ()

Search paths: current directory, -I, METAML_PATH, METAML_LIBPATH, stdlib.
.mml files are programs; sibling .mmli (or .mlli) files declare their interfaces.
Interfaces contain only types and signatures and never runnable definitions.
METAML_PATH and METAML_LIBPATH are colon-separated directory lists.
METAML_SINK redirects language output and results to a file (truncated once).
With no supplied code, read submissions from stdin. Use %help for directives.
)";

Options parse_options (int argc, char **argv)
{
  Options o;
  for (int i = 1; i < argc; ++i)
    {
      std::string a = argv[i], attached;
      bool has_attached = false;
      if (a.starts_with ("--"))
        if (auto eq = a.find ('='); eq != a.npos)
          { attached = a.substr (eq + 1); a.resize (eq); has_attached = true; }
      if (a.starts_with ("-I") && a.size () > 2)
        { attached = a.substr (2); a = "-I"; has_attached = true; }
      auto need = [&] ()
        {
          if (has_attached) { has_attached = false; return attached; }
          if (++i >= argc) throw std::invalid_argument ("option " + a + " needs an argument");
          return std::string (argv[i]);
        };
      if (a == "--")
        { while (++i < argc) o.args.emplace_back (argv[i]); break; }
      if (a == "-h" || a == "--help") o.help = true;
      else if (a == "-V" || a == "--version") o.version = true;
      else if (a == "-i" || a == "--interactive") o.interactive = true;
      else if (a == "-e" || a == "--eval") o.actions.push_back ({ false, need () });
      else if (a == "--type") o.actions.push_back ({ true, need () });
      else if (a == "-l" || a == "--load") o.loads.push_back (need ());
      else if (a == "-I" || a == "--module-path") o.paths.push_back (need ());
      else if (a == "-c" || a == "--check") o.check = true;
      else if (a == "--ast") o.ast = true;
      else if (a == "--trace") o.trace = true;
      else if (a == "--time") o.time = true;
      else if (a == "--stdlib") o.stdlib = need ();
      else if (a == "--engine") o.engine = need ();
      else if (a == "--no-stdlib") o.no_stdlib = true;
      else if (a == "--history-file") o.history_file = need ();
      else if (a == "--no-history") o.no_history = true;
      else if (a == "--no-init") o.no_init = true;
      else if (a == "--color") o.color = need ();
      else if (a == "--diagnostic-format") o.diagnostic_format = need ();
      else if (a == "-q" || a == "--quiet") o.quiet = true;
      else if (!a.empty () && a[0] == '-') throw std::invalid_argument ("unknown option " + a);
      else if (o.file.empty ()) o.file = a;
      else throw std::invalid_argument ("only one main file is allowed; use -- before program arguments");
      if (has_attached) throw std::invalid_argument ("option " + a + " does not take an argument");
    }
  if (o.color != "auto" && o.color != "always" && o.color != "never")
    throw std::invalid_argument ("--color must be auto, always, or never");
  if (!metaml::execution_engine_from_name(o.engine))
    throw std::invalid_argument ("--engine must be xvm, portable, jit, tree, or qbe");
  if (o.diagnostic_format != "text" && o.diagnostic_format != "json")
    throw std::invalid_argument ("--diagnostic-format must be text or json");
  if (o.ast && o.check) throw std::invalid_argument ("--ast and --check are mutually exclusive");
  if ((o.ast || o.check) && o.interactive)
    throw std::invalid_argument ("--interactive cannot be combined with --ast or --check");
  return o;
}

// The checker annotates ASTs and unifies type graphs in place, including functor
// bodies. Clone those graphs (preserving identity and cycles), rather than making
// a shallow copy or replaying definitions that could have side effects.
struct CheckSnapshot
{
  std::map<const metaml::TypeNode *, metaml::TypeP> types;
  std::map<const metaml::Scope *, metaml::ScopeP> scopes;
  std::map<const metaml::TypeDef *, metaml::TypeDefP> defs;
  std::map<const metaml::CtorInfo *, metaml::CtorInfoP> ctors;

  metaml::TypeP copy (const metaml::TypeP &p)
  {
    if (!p) return {};
    auto &q = types[p.get ()];
    if (q) return q;
    q = std::make_shared<metaml::TypeNode> (*p);
    q->link = copy (p->link); q->field = copy (p->field); q->rest = copy (p->rest);
    for (auto &a : q->args) a = copy (a);
    return q;
  }
  metaml::PatP copy (const metaml::PatP &p)
  {
    if (!p) return {};
    auto q = std::make_shared<metaml::Pat> (*p);
    for (auto &a : q->kids) a = copy (a);
    return q;
  }
  metaml::ExprP copy (const metaml::ExprP &p)
  {
    if (!p) return {};
    auto q = std::make_shared<metaml::Expr> (*p);
    for (auto &a : q->kids) a = copy (a);
    for (auto &a : q->pats) a = copy (a);
    for (auto &a : q->guards) a = copy (a);
    for (auto &a : q->clauses) { a.pat = copy (a.pat); a.expr = copy (a.expr); }
    q->cell.reset (); q->prim = nullptr;
    return q;
  }
  metaml::ItemP copy (const metaml::ItemP &p)
  {
    if (!p) return {};
    auto q = std::make_shared<metaml::Item> (*p);
    q->body = copy (p->body); q->value = copy (p->value);
    for (auto &a : q->params) a = copy (a);
    q->sig = copy (p->sig); q->psig = copy (p->psig); q->mexpr = copy (p->mexpr);
    q->cells.clear ();
    return q;
  }
  metaml::SigExprP copy (const metaml::SigExprP &p)
  {
    if (!p) return {};
    auto q = std::make_shared<metaml::SigExpr> (*p);
    for (auto &a : q->items) a = copy (a);
    return q;
  }
  metaml::ModExprP copy (const metaml::ModExprP &p)
  {
    if (!p) return {};
    auto q = std::make_shared<metaml::ModExpr> (*p);
    for (auto &a : q->items) a = copy (a);
    q->arg = copy (p->arg);
    return q;
  }
  metaml::TypeDefP copy (const metaml::TypeDefP &p)
  {
    if (!p) return {};
    auto &q = defs[p.get ()];
    if (q) return q;
    q = std::make_shared<metaml::TypeDef> (*p);
    q->body = copy (p->body);
    for (auto &a : q->params) a = copy (a);
    for (auto &a : q->ctors) a = copy (a);
    return q;
  }
  metaml::CtorInfoP copy (const metaml::CtorInfoP &p)
  {
    if (!p) return {};
    auto &q = ctors[p.get ()];
    if (q) return q;
    q = std::make_shared<metaml::CtorInfo> (*p);
    q->scheme = copy (p->scheme); q->owner = copy (p->owner);
    return q;
  }
  metaml::ScopeP copy (const metaml::ScopeP &p)
  {
    if (!p) return {};
    auto &q = scopes[p.get ()];
    if (q) return q;
    q = std::make_shared<metaml::Scope> ();
    q->prefix = p->prefix; q->parent = copy (p->parent);
    for (const auto &n : p->vars.keys ())
      q->vars.set (n, { copy (p->vars.find (n)->scheme), {} });
    for (const auto &n : p->types.keys ()) q->types.set (n, copy (*p->types.find (n)));
    for (const auto &n : p->ctors.keys ()) q->ctors.set (n, copy (*p->ctors.find (n)));
    for (const auto &n : p->modules.keys ()) q->modules.set (n, copy (*p->modules.find (n)));
    for (const auto &n : p->pending.keys ()) q->pending.set (n, copy (*p->pending.find (n)));
    for (const auto &n : p->functors.keys ())
      {
        auto f = *p->functors.find (n);
        q->functors.set (n, std::make_shared<metaml::FunctorInfo> (
                              metaml::FunctorInfo{ f->name, copy (f->item), copy (f->env) }));
      }
    for (const auto &n : p->sigs.keys ())
      {
        auto s = *p->sigs.find (n);
        q->sigs.set (n, std::make_shared<metaml::SigDef> (
                          metaml::SigDef{ s->name, copy (s->expr), copy (s->env) }));
      }
    return q;
  }
  ~CheckSnapshot ()
  {
    // Module/functor environments and ADT owners form shared_ptr cycles.
    for (auto &[p, q] : scopes) *q = metaml::Scope{};
    for (auto &[p, q] : defs) q->ctors.clear ();
    for (auto &[p, q] : ctors) q->owner.reset ();
  }

  void retain (const metaml::ScopeP &scope, std::set<const metaml::Scope *> &seen)
  {
    // Also retain environments created during checking so their cycles can be
    // broken with the cloned graph when a long-lived REPL discards this check.
    if (!scope || !seen.insert (scope.get ()).second) return;
    scopes.try_emplace (scope.get (), scope);
    retain (scope->parent, seen);
    for (const auto &n : scope->modules.keys ()) retain (*scope->modules.find (n), seen);
    for (const auto &n : scope->functors.keys ()) retain ((*scope->functors.find (n))->env, seen);
    for (const auto &n : scope->sigs.keys ()) retain ((*scope->sigs.find (n))->env, seen);
    for (const auto &n : scope->types.keys ())
      {
        const auto &d = *scope->types.find (n);
        defs.try_emplace (d.get (), d);
        for (const auto &c : d->ctors) ctors.try_emplace (c.get (), c);
      }
  }
};

class StaticCheck
{
  Session &session;
  CheckSnapshot snapshot;
  metaml::Checker checker;
  std::map<metaml::Primitive *, metaml::TypeP> primitive_types;
  std::set<std::string> imported;
  std::map<std::string, std::string> &sources;
public:
  StaticCheck (Session &s, std::map<std::string, std::string> &src) : session (s), sources (src)
  {
    checker.global = snapshot.copy (s.checker ().global);
    checker.next_id = s.checker ().next_id;
    checker.next_stamp = s.checker ().next_stamp;
    checker.disabled = s.checker ().disabled;
    checker.find_prim = [&s] (const std::string &n) { return s.find_primitive (n); };
    checker.prim_type = [this] (metaml::Primitive *p, const dsl::Span &sp)
      {
        auto &t = primitive_types[p];
        if (!t)
          {
            auto parsed = metaml::parse_type_expr (p->type_src, "<primitive " + p->name + ">");
            if (parsed.is_err ()) throw metaml::detail::MetaError{ parsed.unwrap_err ().front () };
            int level = checker.level;
            checker.level = 0;
            try { t = checker.sig_scheme (parsed.unwrap (), checker.global); }
            catch (...) { checker.level = level; throw; }
            checker.level = level;
          }
        (void)sp;
        return t;
      };
    for (const auto &f : s.imported_files ()) imported.insert (f);
  }

  ~StaticCheck ()
  {
    std::set<const metaml::Scope *> seen;
    snapshot.retain (checker.global, seen);
  }

  RunReport source (const std::string &text, const std::string &file, bool disk = false)
  {
    sources[file] = text;
    auto parsed = disk ? metaml::parse_file (file) : metaml::parse (text, file);
    RunReport result;
    if (!parsed.ok ()) { result.errors = parsed.diagnostics; return result; }
    auto original_global = checker.global;
    if (parsed.program.interface)
      {
        sources[parsed.program.interface_file] = parsed.program.interface_source;
        checker.global = metaml::Scope::child (original_global);
      }
    struct Restore
    {
      metaml::Checker &checker;
      metaml::ScopeP saved;
      ~Restore () { checker.global = saved; }
    } restore{ checker, original_global };
    for (auto [b, e] : metaml::Checker::units (parsed.program.items))
      {
        auto item = parsed.program.items[b];
        checker.warnings.clear ();
        try
          {
            if (item->kind == metaml::IK::Import)
              {
                auto path = session.resolve_import (item->text, file);
                if (!path) metaml::detail::fail ("M0505", "cannot find module `" + item->text + "`", item->span);
                if (imported.insert (*path).second)
                  {
                    auto r = this->file (*path);
                    result.warnings.insert (result.warnings.end (), r.warnings.begin (), r.warnings.end ());
                    result.errors.insert (result.errors.end (), r.errors.begin (), r.errors.end ());
                    checker.warnings.clear ();
                  }
              }
            else if (item->kind == metaml::IK::Directive)
              metaml::detail::fail ("M0601", "directives are not allowed in checked source", item->span);
            else
              {
                auto scope = metaml::Scope::child (checker.global);
                auto info = checker.check_unit (parsed.program.items, b, e, scope, nullptr, true);
                checker.u.commit ();
                metaml::Checker::commit_scope (scope, checker.global);
                if (info.expr_type) result.type = info.expr_type;
              }
          }
        catch (const metaml::detail::MetaError &err) { result.errors.push_back (err.diag); }
        checker.u.commit ();
        result.warnings.insert (result.warnings.end (), checker.warnings.begin (), checker.warnings.end ());
        if (!result.ok ()) break;
      }
    if (result.ok () && parsed.program.interface)
      try
        {
          checker.check_ascription (checker.global, parsed.program.interface, original_global,
                                    metaml::Asc::Transparent, "", parsed.program.interface->span);
          checker.u.commit ();
          metaml::Checker::commit_scope (checker.global, original_global);
        }
      catch (const metaml::detail::MetaError &err)
        { checker.u.commit (); result.errors.push_back (err.diag); }
    return result;
  }

  RunReport file (const std::string &path)
  {
    auto text = dsl::load_parse_input_file (path);
    if (text.is_err ())
      {
        RunReport r;
        r.errors.push_back (metaml::detail::make_diag ("M0603", "cannot read file `" + path + "`", {}));
        return r;
      }
    std::error_code ec;
    imported.insert (fs::weakly_canonical (path, ec).string ());
    return source (text.unwrap (), path, true);
  }

  RunReport type (const std::string &text)
  {
    // Parse as an expression, not as a sequence of definitions.
    sources["<type>"] = text;
    RunReport r;
    auto parsed = metaml::parse_expression (text, "<type>");
    if (parsed.is_err ()) { r.errors = parsed.unwrap_err (); return r; }
    auto item = std::make_shared<metaml::Item> ();
    item->kind = metaml::IK::Expr;
    item->body = parsed.unwrap ();
    item->span = item->body->span;
    checker.warnings.clear ();
    try
      {
        auto scope = metaml::Scope::child (checker.global);
        auto info = checker.check_unit ({ item }, 0, 1, scope, nullptr, true);
        r.text = metaml::show_type (info.expr_type) + "\n";
      }
    catch (const metaml::detail::MetaError &err) { r.errors.push_back (err.diag); }
    checker.u.commit ();
    r.warnings = checker.warnings;
    return r;
  }
};

struct Directive
{
  const char *name, *args, *help;
  bool required;
};
const Directive directives[] = {
  { "help", "[DIRECTIVE]", "List directives or explain one.", false },
  { "type", "EXPR", "Infer an expression's type without evaluation.", true },
  { "load", "FILE", "Load and evaluate a file; remember it for %reload.", true },
  { "reload", "", "Evaluate the last loaded file again.", false },
  { "ast", "SOURCE", "Display the parsed abstract syntax tree.", true },
  { "trace", "EXPR", "Evaluate once and display the interpreter's reduction/call trace.", true },
  { "stack", "", "Display the implicit value stack.", false },
  { "clear", "", "Empty the implicit value stack.", false },
  { "bindings", "[PREFIX]", "List visible values, constructors, and their types.", false },
  { "inspect", "NAME", "Show a binding's type, value, and available definition location.", true },
  { "check", "FILE", "Check a file and its imports in an isolated copy of the current type environment.", true },
  { "time", "EXPR", "Evaluate once and report elapsed time to stderr.", true },
  { "history", "", "Show numbered submissions from this REPL session.", false },
  { "reset", "", "Restore the initial library environment; clear definitions and stack.", false },
  { "quit", "", "Exit the REPL (also Ctrl-D).", false },
  { "paths", "[DIR]", "Show search directories, or append a module search directory.", false },
  { "which", "FILE", "Resolve a source file or module using the search path.", true },
  { "modules", "", "List loaded source files.", false },
  { "pwd", "", "Show the working directory.", false },
  { "cd", "DIR", "Change the working directory (supports ~).", true },
};

class Frontend
{
  Options opt;
  std::ofstream sink;
  std::ostream *out = &std::cout;
  std::unique_ptr<Session> session;
  std::unique_ptr<StaticCheck> static_check;
  std::map<std::string, std::string> sources;
  std::map<std::string, dsl::Span> definitions;
  std::vector<std::string> history, extra_paths;
  std::string last_loaded, stdlib_dir;
  bool quit = false;
  int status = 0, submission = 0;

  bool color (int fd) const
  { return opt.color == "always" || (opt.color == "auto" && tty (fd) && !std::getenv ("NO_COLOR")); }

  void diagnostic (const dsl::Diagnostic &d)
  {
    const auto &s = d.primary_span;
    if (opt.diagnostic_format == "json")
      {
        std::cerr << "{\"severity\":" << json_string (dsl::severity_name (d.severity))
                  << ",\"code\":" << json_string (d.code) << ",\"message\":" << json_string (d.message)
                  << ",\"file\":" << json_string (s.file) << ",\"line\":" << s.line
                  << ",\"column\":" << s.column << ",\"begin\":" << s.begin << ",\"end\":" << s.end
                  << ",\"suggestions\":[";
        for (std::size_t i = 0; i < d.suggestions.size (); ++i)
          std::cerr << (i ? "," : "") << json_string (d.suggestions[i]);
        std::cerr << "]}\n";
      }
    else
      {
        auto it = sources.find (s.file);
        std::string rendered = it != sources.end () ? dsl::render_diagnostic (d, it->second)
                                                    : session ? session->render (d) : dsl::render_diagnostic (d);
        if (color (2)) std::cerr << (d.severity == dsl::Severity::Error ? "\033[31m" : "\033[33m");
        std::cerr << rendered;
        if (color (2)) std::cerr << "\033[0m";
      }
    if (d.severity == dsl::Severity::Error) status = 1;
  }

  bool error (const std::string &message, const std::string &code = "M0601")
  { diagnostic (metaml::detail::make_diag (code, message, {})); return false; }

  bool report (RunReport r, bool text = true)
  {
    // Repl/Quiet reports embed warnings in text as well as in warnings.
    for (const auto &w : r.warnings)
      {
        auto rendered = session->render (w);
        if (auto p = r.text.find (rendered); p != r.text.npos) r.text.erase (p, rendered.size ());
      }
    if (text) *out << r.text;
    out->flush ();
    for (const auto &w : r.warnings) diagnostic (w);
    for (const auto &e : r.errors) diagnostic (e);
    return r.ok ();
  }

  std::optional<std::string> resolve (const std::string &name)
  { return session->resolve_import (unquote (name)); }

  std::string find_stdlib ()
  {
    auto candidates = opt.stdlib.empty () ? env_paths ("METAML_LIBPATH") : std::vector<std::string>{ unquote (opt.stdlib) };
    if (opt.stdlib.empty ())
      {
        auto defaults = session->stdlib_candidates ();
        candidates.insert (candidates.end (), defaults.begin (), defaults.end ());
      }
    for (const auto &dir : candidates)
      {
        std::error_code ec;
        if (fs::is_regular_file (fs::path (dir) / "prelude/mod.mml", ec))
          return fs::absolute (dir).lexically_normal ().string ();
      }
    return {};
  }

  bool initialize ()
  {
    static_check.reset ();
    session = std::make_unique<Session> ();
    session->interp().engine = *metaml::execution_engine_from_name(opt.engine);
    session->set_output ([this] (std::string_view s) { *out << s; out->flush (); });
    session->allow_all_primitives ();
    session->args = opt.args;
    session->set_max_depth (100000);
    // Program arguments are available even without metalib.
    session->register_primitive ("Process.args", "Unit -> [String]", 1,
      [] (metaml::PrimContext &c, std::vector<metaml::Value> &)
        {
          std::vector<metaml::Value> values;
          for (auto &a : c.session.args) values.push_back (metaml::make_str (a));
          return metaml::make_list (values);
        });
    for (const auto &p : opt.paths) session->add_search_path (unquote (p));
    for (const auto *env : { "METAML_PATH", "METAML_LIBPATH" })
      for (const auto &p : env_paths (env)) session->add_search_path (p);
    for (const auto &p : extra_paths) session->add_search_path (p);
    stdlib_dir.clear ();
    if (!opt.no_stdlib && !opt.ast)
      {
        metalib::install_all (*session);
        stdlib_dir = find_stdlib ();
        if (stdlib_dir.empty ()) return error ("cannot locate metalib; use --stdlib or METAML_LIBPATH", "M0505");
        session->add_search_path (stdlib_dir);
        if (!opt.check)
          {
            auto r = session->load_stdlib (stdlib_dir);
            if (r.is_err ()) { for (const auto &d : r.unwrap_err ()) diagnostic (d); return false; }
          }
      }
    if (opt.check)
      {
        static_check = std::make_unique<StaticCheck> (*session, sources);
        if (!stdlib_dir.empty () && !report (static_check->file (stdlib_dir + "/prelude/mod.mml"))) return false;
      }
    return true;
  }

  void remember_definitions (const metaml::Program &p)
  {
    // Metadata is supplemental; only bindings that actually exist are inspected.
    std::function<void (const std::vector<metaml::ItemP> &, const std::string &)> walk;
    walk = [&] (const auto &items, const std::string &prefix)
      {
        for (const auto &i : items)
          {
            if (i->kind == metaml::IK::Def || i->kind == metaml::IK::Module)
              definitions[prefix + i->name] = i->span;
            if (i->mexpr && i->mexpr->kind == metaml::MK::Struct)
              walk (i->mexpr->items, prefix + i->name + ".");
          }
      };
    walk (p.items, "");
  }

  bool evaluate (const std::string &source, const std::string &file, bool repl,
                 bool print_result = true, bool timed = false, bool traced = false, bool from_file = false)
  {
    sources[file] = source;
    if (opt.check) return report (static_check->source (source, file));
    auto parsed = from_file ? metaml::parse_file (file) : metaml::parse (source, file);
    if (!parsed.ok ()) { for (const auto &d : parsed.diagnostics) diagnostic (d); return false; }
    if (opt.ast) { *out << metaml::dump_ast (parsed.program) << '\n'; return true; }
    auto start = std::chrono::steady_clock::now ();
    auto &interp = session->interp ();
    bool old_trace = interp.tracing;
    auto old_sink = interp.trace_sink;
    interp.tracing = traced || opt.trace;
    interp.trace_base = interp.call_depth;
    interp.trace_sink = [this] (const std::string &s) { std::cerr << s << '\n'; };
    struct RestoreTrace
    {
      metaml::Interp &interp;
      bool enabled;
      std::function<void (const std::string &)> sink;
      ~RestoreTrace () { interp.tracing = enabled; interp.trace_sink = std::move (sink); }
    } restore{ interp, old_trace, std::move (old_sink) };
    auto mode = repl ? metaml::RunMode::Repl : metaml::RunMode::Eval;
    auto r = from_file ? session->run_file (file, mode) : session->run_program (parsed.program, mode);
    if (r.ok ()) remember_definitions (parsed.program);
    if (repl && opt.quiet)
      {
        std::istringstream lines (r.text);
        std::string line, filtered;
        while (std::getline (lines, line))
          if (!line.starts_with ("val ") && !line.starts_with ("type ") && !line.starts_with ("module ")
              && !line.starts_with ("functor ")) filtered += line + '\n';
        r.text = std::move (filtered);
      }
    if (!repl && print_result && r.ok () && r.value) *out << metaml::show_value (*r.value) << '\n';
    bool ok = report (r, print_result);
    if (timed || opt.time)
      {
        double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now () - start).count ();
        std::cerr << "Elapsed: " << ms << " ms\n";
      }
    return ok;
  }

  bool load (const std::string &name, bool print_result = true)
  {
    auto path = resolve (name);
    if (!path) return error ("cannot find file `" + unquote (name) + "`", "M0603");
    auto text = dsl::load_parse_input_file (*path);
    if (text.is_err ()) return error ("cannot read file `" + *path + "`", "M0603");
    bool ok = opt.check ? report (static_check->file (*path)) : evaluate (text.unwrap (), *path, false, print_result, false, false, true);
    if (ok && !opt.check && !opt.ast) last_loaded = *path;
    return ok;
  }

  metaml::ScopeP binding_scope (const std::string &name, std::string &leaf)
  {
    auto scope = session->checker ().global;
    std::size_t b = 0;
    while (true)
      {
        auto dot = name.find ('.', b);
        if (dot == name.npos) { leaf = name.substr (b); return scope; }
        auto m = scope->module (name.substr (b, dot - b));
        if (!m) return {};
        scope = *m; b = dot + 1;
      }
  }

  bool bindings (const std::string &prefix, bool inspect)
  {
    auto names = inspect ? std::vector<std::string>{ prefix } : session->completions (prefix);
    bool found = false;
    for (const auto &name : names)
      {
        std::string leaf, type;
        auto scope = binding_scope (name, leaf);
        const metaml::VarInfo *v = scope ? scope->var (leaf) : nullptr;
        const auto *c = scope ? scope->ctor (leaf) : nullptr;
        auto prim = session->find_primitive (name);
        if (v) type = metaml::show_type (v->scheme);
        else if (c) type = metaml::show_type ((*c)->scheme);
        else if (prim) type = prim->type_src;
        else continue;
        found = true;
        *out << name << " : " << type << '\n';
        if (!inspect) continue;
        // Inspect already materialized values only: resolving a lazy module can
        // execute its body, which would make inspection unexpectedly effectful.
        auto value = session->global (name);
        if (!value && v && v->cell && v->cell->defined) value = v->cell->value;
        if (!value && name.find ('.') != name.npos)
          {
            std::vector<std::string> parts;
            std::istringstream path (name); std::string part;
            while (std::getline (path, part, '.')) parts.push_back (part);
            auto frame = session->root ();
            metaml::ModuleP mod;
            for (const auto &entry : frame->mods) if (entry.first == parts[0]) mod = entry.second;
            for (std::size_t i = 1; mod && i + 1 < parts.size (); ++i)
              {
                metaml::ModuleP next;
                for (const auto &entry : mod->mods) if (entry.first == parts[i]) next = entry.second;
                mod = next;
              }
            if (mod) for (const auto &entry : mod->fields) if (entry.first == parts.back ()) value = entry.second;
          }
        *out << "  value: " << (value ? metaml::show_value (*value) : prim ? "<primitive>" : "<not materialized>") << '\n';
        if (auto d = definitions.find (name); d != definitions.end ())
          *out << "  defined at " << d->second.file << ':' << d->second.line << ':' << d->second.column << '\n';
        else if (value && value->kind == metaml::VK::Closure)
          {
            const auto &sp = std::static_pointer_cast<metaml::ClosureObj> (value->obj)->lam->span;
            *out << "  body at " << sp.file << ':' << sp.line << ':' << sp.column << '\n';
          }
        if (prim) *out << "  primitive arity: " << prim->arity << '\n';
      }
    if (inspect && !found) return error ("unknown binding `" + prefix + "`");
    if (!found) *out << "(no matching bindings)\n";
    return true;
  }

  bool directive (const std::string &line)
  {
    auto end = line.find_first_of (" \t\r\n");
    std::string name = line.substr (1, end == line.npos ? end : end - 1);
    std::string arg = end == line.npos ? "" : trim (line.substr (end));
    const Directive *spec = nullptr;
    for (const auto &d : directives) if (name == d.name) spec = &d;
    if (!spec) return error ("unknown directive `%" + name + "`; try %help");
    if ((spec->required && arg.empty ()) || (!*spec->args && !arg.empty ()))
      return error ("usage: %" + name + (*spec->args ? " " + std::string (spec->args) : ""));
    if (name == "help")
      {
        if (arg.starts_with ('%')) arg.erase (0, 1);
        bool found = false;
        for (const auto &d : directives)
          if (arg.empty () || arg == d.name)
            { *out << '%' << d.name << (*d.args ? " " : "") << d.args << "\n  " << d.help << '\n'; found = true; }
        return found || error ("unknown directive `%" + arg + "`");
      }
    if (name == "quit") { quit = true; return true; }
    if (name == "load") return load (arg);
    if (name == "reload") return last_loaded.empty () ? error ("nothing to reload; use %load first", "M0602") : load (last_loaded);
    if (name == "type") { StaticCheck check (*session, sources); return report (check.type (arg)); }
    if (name == "check")
      {
        auto path = resolve (arg);
        if (!path) return error ("cannot find file `" + unquote (arg) + "`", "M0603");
        StaticCheck check (*session, sources);
        bool ok = report (check.file (*path));
        if (ok) *out << "Checked " << *path << '\n';
        return ok;
      }
    if (name == "ast" || name == "stack" || name == "clear") return report (session->directive (name, arg));
    if (name == "time" || name == "trace")
      {
        sources["<" + name + ">"] = arg;
        auto parsed = metaml::parse_expression (arg, "<" + name + ">");
        if (parsed.is_err ()) { for (const auto &d : parsed.unwrap_err ()) diagnostic (d); return false; }
        return evaluate (arg, "<" + name + ">", true, true, name == "time", name == "trace");
      }
    if (name == "bindings" || name == "inspect") return bindings (arg, name == "inspect");
    if (name == "history")
      { for (std::size_t i = 0; i < history.size (); ++i) *out << i + 1 << "  " << history[i] << '\n'; return true; }
    if (name == "reset")
      {
        definitions.clear (); sources.clear (); last_loaded.clear ();
        bool ok = initialize ();
        if (ok) *out << "Session reset.\n";
        return ok;
      }
    if (name == "paths")
      {
        if (!arg.empty ()) { extra_paths.push_back (unquote (arg)); session->add_search_path (extra_paths.back ()); }
        *out << fs::current_path ().string () << " (working directory)\n";
        std::set<std::string> seen;
        for (auto &p : session->search_paths ()) if (seen.insert (p).second) *out << p << '\n';
        return true;
      }
    if (name == "which")
      { auto p = resolve (arg); if (!p) return error ("cannot resolve `" + unquote (arg) + "`"); *out << *p << '\n'; return true; }
    if (name == "modules")
      {
        auto files = session->imported_files ();
        if (!last_loaded.empty () && std::find (files.begin (), files.end (), last_loaded) == files.end ()) files.push_back (last_loaded);
        for (const auto &p : files) *out << p << '\n';
        return true;
      }
    if (name == "pwd") { *out << fs::current_path ().string () << '\n'; return true; }
    if (name == "cd")
      { std::error_code ec; fs::current_path (unquote (arg), ec); return ec ? error (ec.message (), "M0603") : true; }
    return false;
  }

  static int codepoints (std::string_view s)
  {
    return static_cast<int> (std::count_if (s.begin (), s.end (), [] (unsigned char c) { return (c & 0xc0) != 0x80; }));
  }

  Replxx::completions_t complete (const std::string &input, int &context)
  {
    std::size_t begin = input.size ();
    while (begin && (metaml::detail::is_ident_char (input[begin - 1]) || input[begin - 1] == '.'
                     || input[begin - 1] == '%' || static_cast<unsigned char> (input[begin - 1]) >= 128)) --begin;
    std::string prefix = input.substr (begin);
    auto first = input.find_first_not_of (" \t");
    auto trimmed = first == input.npos ? std::string () : input.substr (first);
    auto sep = trimmed.find_first_of (" \t");
    std::string cmd = trimmed.substr (0, sep);
    bool file = sep != trimmed.npos && (cmd == "%load" || cmd == "%check" || cmd == "%which" || cmd == "%cd" || cmd == "%paths" || cmd == "import");
    std::set<std::string> matches;
    if (file)
      {
        begin = input.find_first_not_of (" \t", input.find_first_of (" \t", input.find_first_not_of (" \t")));
        if (begin == input.npos) begin = input.size ();
        char quote = begin < input.size () && (input[begin] == '"' || input[begin] == '\'') ? input[begin++] : 0;
        prefix = input.substr (begin);
        fs::path typed (unquote (prefix));
        auto parent = typed.parent_path ();
        if (prefix.ends_with ('/')) parent = typed;
        std::string base = prefix.ends_with ('/') ? "" : typed.filename ().string ();
        auto roots = session->search_paths ();
        roots.insert (roots.begin (), ".");
        if (typed.is_absolute ()) roots = { "" };
        for (const auto &root : roots)
          {
            std::error_code ec;
            fs::directory_iterator entries (fs::path (root) / parent, ec);
            for (auto it = entries; !ec && it != fs::directory_iterator{}; it.increment (ec))
              {
                auto name = it->path ().filename ().string ();
                if (!name.starts_with (base)) continue;
                bool dir = it->is_directory (ec);
                if ((cmd == "%cd" || cmd == "%paths") && !dir) continue;
                std::string candidate = prefix.substr (0, prefix.size () - base.size ()) + name;
                if (dir) candidate += '/';
                else if (quote) candidate += quote;
                matches.insert (candidate);
              }
          }
      }
    else if (prefix.starts_with ('%'))
      { for (auto &d : directives) if (("%" + std::string (d.name)).starts_with (prefix)) matches.insert ("%" + std::string (d.name)); }
    else if (cmd == "%help" && sep != trimmed.npos)
      { for (auto &d : directives) if (std::string (d.name).starts_with (prefix)) matches.insert (d.name); }
    else
      { for (auto &s : session->completions (prefix)) matches.insert (s); }
    context = codepoints (std::string_view (input).substr (begin));
    Replxx::completions_t result;
    for (auto &s : matches) result.emplace_back (s);
    return result;
  }

  void highlight (const std::string &input, Replxx::colors_t &colors)
  {
    using C = Replxx::Color;
    // Token spans are bytes; replxx colors are indexed by Unicode code point.
    std::vector<std::size_t> offsets (input.size () + 1);
    std::size_t cp = 0;
    for (std::size_t i = 0; i < input.size (); ++i)
      { offsets[i] = cp; if ((static_cast<unsigned char> (input[i]) & 0xc0) != 0x80) ++cp; }
    offsets.back () = cp;
    auto paint = [&] (std::size_t b, std::size_t e, C c)
      { for (auto i = offsets[std::min (b, input.size ())]; i < offsets[std::min (e, input.size ())] && i < colors.size (); ++i) colors[i] = c; };
    auto lexed = metaml::lex (input);
    // Trivia consists of whitespace and comments. Paint it gray, then color tokens.
    paint (0, input.size (), C::GRAY);
    for (const auto &t : lexed.tokens)
      {
        C c = C::DEFAULT;
        switch (t.kind)
          {
          case metaml::Tok::Kw: c = C::YELLOW; break;
          case metaml::Tok::Str: c = C::GREEN; break;
          case metaml::Tok::Int: case metaml::Tok::Float: c = C::CYAN; break;
          case metaml::Tok::UIdent: case metaml::Tok::TyVar: c = C::BRIGHTBLUE; break;
          case metaml::Tok::Op: case metaml::Tok::Tacit: case metaml::Tok::Hole: c = C::MAGENTA; break;
          case metaml::Tok::Directive: c = C::BRIGHTMAGENTA; break;
          default: break;
          }
        paint (t.span.begin, t.span.end, c);
      }
    auto first = input.find_first_not_of (" \t");
    if (first != input.npos && input[first] == '%')
      {
        auto end = input.find_first_of (" \t\n", first);
        paint (first, end == input.npos ? input.size () : end, C::BRIGHTMAGENTA);
      }
  }

  std::string history_path ()
  {
    if (!opt.history_file.empty ()) return unquote (opt.history_file);
    if (const char *p = std::getenv ("XDG_STATE_HOME"); p && *p) return std::string (p) + "/metaml/history";
    if (const char *p = std::getenv ("HOME"); p && *p) return std::string (p) + "/.local/state/metaml/history";
    return {};
  }

  bool init_file (const std::string &path)
  {
    std::ifstream input (path);
    if (!input) return error ("cannot read initialization file `" + path + "`", "M0603");
    std::string line, pending;
    std::size_t line_number = 0;
    while (std::getline (input, line))
      {
        ++line_number;
        if (pending.empty () && trim (line).empty ()) continue;
        if (pending.empty () && trim (line).starts_with ('%'))
          { if (!directive (trim (line))) return false; if (quit) return true; continue; }
        if (!pending.empty ()) pending += '\n';
        else pending.assign (line_number - 1, '\n');
        pending += line;
        auto parsed = metaml::parse (pending, path);
        if (!parsed.ok () && parsed.incomplete) continue;
        if (!evaluate (pending, path, false, false)) return false;
        pending.clear ();
      }
    return pending.empty () || evaluate (pending, path, false, false);
  }

  void repl ()
  {
    bool terminal = tty (0) && tty (1);
    Replxx editor;
    editor.set_max_history_size (2000);
    editor.set_completion_callback ([this] (const std::string &s, int &n) { return complete (s, n); });
    if (color (1)) editor.set_highlighter_callback ([this] (const std::string &s, auto &c) { highlight (s, c); });
    std::string hist = opt.no_history || !terminal ? "" : history_path ();
    if (!hist.empty ()) editor.history_load (hist);
    if (terminal && !opt.quiet)
      std::cout << "MetaML " << metaml::version << " — %help for directives, %quit to exit\n";
    std::string pending;
    while (!quit && !session->quit_requested ())
      {
        std::string line;
        if (terminal)
          {
            errno = 0;
            const char *input = editor.input (pending.empty () ? "metaml> " : "     .. ");
            if (!input)
              {
                if (errno == EAGAIN || errno == EINTR) { pending.clear (); continue; }
                break;
              }
            line = input;
          }
        else if (!std::getline (std::cin, line)) break;
        if (pending.empty () && trim (line).empty ()) continue;
        if (pending.empty () && trim (line).starts_with ('%'))
          {
            directive (trim (line));
            history.push_back (line); editor.history_add (line); out->flush ();
            continue;
          }
        if (!pending.empty ()) pending += '\n';
        pending += line;
        auto parsed = metaml::parse (pending, "<repl:" + std::to_string (submission + 1) + ">");
        if (!parsed.ok () && parsed.incomplete) continue;
        history.push_back (pending); editor.history_add (pending);
        evaluate (pending, "<repl:" + std::to_string (++submission) + ">", true);
        pending.clear ();
      }
    if (!pending.empty ()) evaluate (pending, "<repl:" + std::to_string (++submission) + ">", true);
    if (!hist.empty ())
      {
        std::error_code ec;
        auto parent = fs::path (hist).parent_path ();
        if (!parent.empty ()) fs::create_directories (parent, ec);
        if (ec || !editor.history_sync (hist)) error ("cannot save history to `" + hist + "`", "M0603");
      }
    if (terminal) std::cout << '\n';
  }

public:
  explicit Frontend (Options o) : opt (std::move (o)) {}

  int run ()
  {
    if (const char *p = std::getenv ("METAML_SINK"); p && *p)
      {
        sink.open (p, std::ios::out | std::ios::trunc);
        if (!sink) { error ("cannot open METAML_SINK `" + std::string (p) + "`", "M0603"); return 1; }
        sink.exceptions (std::ios::badbit | std::ios::failbit);
        out = &sink;
      }
    if (!initialize ()) return 1;
    bool supplied = !opt.file.empty () || !opt.actions.empty () || !opt.loads.empty ();
    bool interactive = opt.interactive || (!supplied && !opt.check && !opt.ast);
    if (interactive && !opt.no_init)
      if (const char *home = std::getenv ("HOME"))
        {
          fs::path init = fs::path (home) / ".metamlrc";
          std::error_code ec;
          if (fs::is_regular_file (init, ec) && !init_file (init.string ())) return 1;
        }
    if (quit) return status;
    bool ok = true;
    for (const auto &file : opt.loads) if (!(ok = load (file, false))) break;
    if (ok && !opt.file.empty ()) ok = load (opt.file);
    if (ok)
      for (const auto &action : opt.actions)
        {
          if (action.type)
            {
              if (static_check) ok = report (static_check->type (action.source));
              else { StaticCheck check (*session, sources); ok = report (check.type (action.source)); }
            }
          else ok = evaluate (action.source, "<eval:" + std::to_string (++submission) + ">", false);
          if (!ok) break;
        }
    if (!supplied && (opt.check || opt.ast))
      {
        std::string source ((std::istreambuf_iterator<char> (std::cin)), {});
        evaluate (source, "<stdin>", false);
      }
    if (interactive) repl ();
    session->close ();
    out->flush ();
    return status;
  }
};

int run (int argc, char **argv)
{
  Options options;
  try { options = parse_options (argc, argv); }
  catch (const std::invalid_argument &e)
    {
      // Honor JSON even when option parsing itself fails.
      bool json = false;
      for (int i = 1; i < argc; ++i)
        if (std::string (argv[i]) == "--diagnostic-format=json"
            || (std::string (argv[i]) == "--diagnostic-format" && i + 1 < argc && std::string (argv[i + 1]) == "json")) json = true;
      if (json) std::cerr << "{\"severity\":\"error\",\"code\":\"CLI\",\"message\":" << json_string (e.what ()) << "}\n";
      else std::cerr << "metaml: " << e.what () << "\nTry 'metaml --help'.\n";
      return 2;
    }
  if (options.help) { std::cout << usage; return 0; }
  if (options.version) { std::cout << "MetaML " << metaml::version << " (" << metaml::dialect << ")\n"; return 0; }
  bool json = options.diagnostic_format == "json";
  try { return Frontend (std::move (options)).run (); }
  catch (const std::exception &e)
    {
      if (json) std::cerr << "{\"severity\":\"error\",\"code\":\"CLI\",\"message\":" << json_string (e.what ()) << "}\n";
      else std::cerr << "metaml: " << e.what () << '\n';
      return 1;
    }
}
} // namespace

int main (int argc, char **argv)
{
#if !defined(_WIN32)
  // Match the existing frontend's stack allowance for non-tail ML recursion.
  struct Args { int argc; char **argv; int status = 1; } args{ argc, argv };
  pthread_attr_t attr;
  if (pthread_attr_init (&attr) == 0)
    {
      if (pthread_attr_setstacksize (&attr, std::size_t{1} << 30) == 0)
        {
          pthread_t thread;
          auto entry = [] (void *p) -> void *
            { auto &a = *static_cast<Args *> (p); a.status = run (a.argc, a.argv); return nullptr; };
          int created = pthread_create (&thread, &attr, entry, &args);
          pthread_attr_destroy (&attr);
          if (created == 0) { pthread_join (thread, nullptr); return args.status; }
        }
      else pthread_attr_destroy (&attr);
    }
#endif
  return run (argc, argv);
}
