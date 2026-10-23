// repl/metaml.cpp — the MetaML REPL (C++ dialect).
//
// A thin wrapper over dsltk::repl::Session (DSLtk.hpp) instantiated with the
// metaml::ReplLanguage policy (MetaML.hpp). Mirrors repl/metaml.d exactly.
//
//   metaml [options] [file.ml ...]
//
// Build: c++ -std=c++20 -O2 -I.. metaml.cpp -o metaml [-DMETAML_USE_READLINE -lreadline]

#include "../MetaML.hpp"

#if __has_include("../metalib/cpp/Metalib.h")
#include "../metalib/cpp/Metalib.h"
#define METAML_HAVE_METALIB 1
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(METAML_USE_READLINE)
#include <readline/history.h>
#include <readline/readline.h>
#endif

namespace
{

struct Options
{
  bool no_tty = false;
  bool no_history = false;
  bool no_stdlib = false;
  bool list_primitives = false;
  bool interactive_after = false;
  std::string stdlib;
  std::string dump_ast;
  std::vector<std::string> disabled;
  std::vector<std::string> exprs;
  std::vector<std::string> files;
};

const char *usage_text
    = "usage: metaml [options] [file.ml ...]\n"
      "  --no-tty          never print prompts\n"
      "  --no-history      do not read or write the history file\n"
      "  --no-stdlib       do not load metalib\n"
      "  --stdlib DIR      use DIR as the metalib directory\n"
      "  --dump-ast FILE   print the AST of FILE and exit\n"
      "  --list-primitives print registered name, arity and type (TSV)\n"
      "  --disable CODE    disable a warning (e.g. M0307)\n"
      "  -e EXPR           evaluate EXPR as REPL input and exit\n"
      "  -i                start the REPL after running files\n"
      "  --version         print the version and exit\n";

std::string
history_path ()
{
  std::string base;
  if (const char *x = std::getenv ("XDG_STATE_HOME"); x && *x)
    base = x;
  else if (const char *h = std::getenv ("HOME"); h && *h)
    base = std::string (h) + "/.local/state";
  else
    return "";
  std::error_code ec;
  std::filesystem::create_directories (base + "/metaml", ec);
  return base + "/metaml/history";
}

long long
mtime_of (const std::string &path)
{
  std::error_code ec;
  auto t = std::filesystem::last_write_time (path, ec);
  if (ec)
    return -1;
  return static_cast<long long> (t.time_since_epoch ().count ());
}

#if defined(METAML_USE_READLINE)
dsl::repl::Session<metaml::ReplLanguage> *g_repl = nullptr;
std::vector<std::string> g_matches;

char *
completion_generator (const char *text, int state)
{
  if (state == 0)
    g_matches = g_repl->complete (text);
  if (static_cast<std::size_t> (state) < g_matches.size ())
    return strdup (g_matches[static_cast<std::size_t> (state)].c_str ());
  return nullptr;
}

char **
completion (const char *text, int, int)
{
  rl_attempted_completion_over = 1;
  return rl_completion_matches (text, completion_generator);
}
#endif

int
run (int argc, char **argv)
{
  Options opt;
  for (int i = 1; i < argc; ++i)
    {
      std::string a = argv[i];
      auto need = [&] () -> std::string
        {
          if (i + 1 >= argc)
            {
              std::cerr << "metaml: option " << a << " needs an argument\n";
              std::exit (2);
            }
          return argv[++i];
        };
      if (a == "--no-tty")
        opt.no_tty = true;
      else if (a == "--no-history")
        opt.no_history = true;
      else if (a == "--no-stdlib")
        opt.no_stdlib = true;
      else if (a == "--list-primitives")
        opt.list_primitives = true;
      else if (a == "--stdlib")
        opt.stdlib = need ();
      else if (a == "--dump-ast")
        opt.dump_ast = need ();
      else if (a == "--disable")
        opt.disabled.push_back (need ());
      else if (a == "-e")
        opt.exprs.push_back (need ());
      else if (a == "-i")
        opt.interactive_after = true;
      else if (a == "--version")
        {
          std::cout << "MetaML " << metaml::version << " (" << metaml::dialect << ")\n";
          return 0;
        }
      else if (a == "--help" || a == "-h")
        {
          std::cout << usage_text;
          return 0;
        }
      else if (!a.empty () && a[0] == '-')
        {
          std::cerr << "metaml: unknown option " << a << "\n" << usage_text;
          return 2;
        }
      else
        opt.files.push_back (a);
    }

  if (!opt.dump_ast.empty ())
    {
      auto text = dsl::load_parse_input_file (opt.dump_ast);
      if (text.is_err ())
        {
          std::cerr << "metaml: cannot read " << opt.dump_ast << "\n";
          return 1;
        }
      auto parsed = metaml::parse (text.unwrap (), opt.dump_ast);
      std::cout << metaml::dump_ast (parsed.program) << "\n";
      for (auto &d : parsed.diagnostics)
        std::cout << dsl::render_diagnostic (d, text.unwrap ());
      return parsed.ok () ? 0 : 1;
    }

  metaml::Session session;
  session.allow_all_primitives ();
  session.set_max_depth (100000);
  for (auto &c : opt.disabled)
    session.disable_warning (c);
  if (const char *d = std::getenv ("METAML_TRACE_DEPTH"))
    session.set_trace_depth (std::atoi (d));
#ifdef METAML_HAVE_METALIB
  metalib::install_all (session);
#endif
  if (!opt.no_stdlib)
    {
      auto r = session.load_stdlib (opt.stdlib);
      if (r.is_err ())
        {
          for (auto &d : r.unwrap_err ())
            std::cerr << session.render (d);
          return 1;
        }
    }

  if (opt.list_primitives)
    {
      for (const auto &name : session.primitive_names ())
        {
          auto *p = session.find_primitive (name);
          std::cout << name << '\t' << p->arity << '\t' << p->type_src << '\n';
        }
      return 0;
    }
  int status = 0;
  for (auto &f : opt.files)
    {
      auto r = session.run_file (f, metaml::RunMode::Quiet);
      std::cout << r.text << std::flush;
      for (auto &w : r.warnings)
        std::cerr << session.render (w);
      for (auto &d : r.errors)
        {
          std::cout << std::flush;
          std::cerr << session.render (d) << std::flush;
        }
      if (!r.ok ())
        {
          status = 1;
          break;
        }
    }

  metaml::ReplLanguage lang{ session };
  dsl::repl::Session<metaml::ReplLanguage> repl (lang, std::cout);
  repl.set_prompts ("meaml> ", ".. > ");
  metaml::install_repl_directives (repl, session);

  if (!opt.exprs.empty ())
    {
      for (auto &e : opt.exprs)
        {
          repl.feed_multiline (e);
          repl.finish ();
        }
      return repl.error_count () ? 1 : status;
    }
  if (!opt.files.empty () && !opt.interactive_after)
    return status;

  bool tty = !opt.no_tty;
#if defined(__unix__) || defined(__APPLE__)
  tty = tty && isatty (0) && isatty (1);
#endif
  std::string hist = (!opt.no_history && tty) ? history_path () : std::string ();
  if (!hist.empty ())
    repl.load_history (hist);

  // live reload (§18.5): re-load the last #load-ed file when it changes
  std::string watched;
  long long watched_time = -1;
  auto check_reload = [&] ()
    {
      const std::string &last = session.last_loaded ();
      if (last.empty ())
        return;
      long long t = mtime_of (last);
      if (watched != last)
        {
          watched = last;
          watched_time = t;
          return;
        }
      if (t != watched_time && t >= 0)
        {
          watched_time = t;
          auto r = session.directive ("reload", "");
          std::cout << "(reloaded " << last << ")\n" << r.text;
          for (auto &d : r.errors)
            std::cout << session.render (d);
        }
    };

  dsl::repl::LineReader reader;
#if defined(METAML_USE_READLINE)
  if (tty)
    {
      g_repl = &repl;
      rl_attempted_completion_function = completion;
      rl_basic_word_break_characters = const_cast<char *> (" \t\n\"\\'`@$><=;|&{(,[");
      for (auto &h : repl.history ())
        add_history (h.c_str ());
      reader = [&] (const std::string &prompt) -> std::optional<std::string>
        {
          check_reload ();
          char *line = readline (prompt.c_str ());
          if (!line)
            return std::nullopt;
          std::string s (line);
          free (line);
          if (!s.empty ())
            add_history (s.c_str ());
          return s;
        };
    }
#endif
  if (!reader && tty)
    reader = [&] (const std::string &prompt) -> std::optional<std::string>
      {
        check_reload ();
        std::cout << prompt << std::flush;
        std::string l;
        if (!std::getline (std::cin, l))
          return std::nullopt;
        return l;
      };
  if (tty)
    std::cout << "MetaML " << metaml::version << " (" << metaml::dialect
              << ") — #help for directives, #quit to leave\n";
  repl.run (std::cin, tty, reader);
  if (!hist.empty ())
    repl.save_history (hist);
  if (tty)
    std::cout << "\n";
  return repl.error_count () ? 1 : status;
}

} // namespace

int
main (int argc, char **argv)
{
  std::ios::sync_with_stdio (false);
#if defined(__unix__) || defined(__APPLE__)
  // Run on a thread with a large stack so deep (non-tail) recursion works.
  struct Args
  {
    int argc;
    char **argv;
    int status;
  } args{ argc, argv, 0 };
  pthread_attr_t attr;
  pthread_attr_init (&attr);
  pthread_attr_setstacksize (&attr, static_cast<std::size_t> (1) << 30);
  pthread_t th;
  auto entry = [] (void *p) -> void *
    {
      auto *a = static_cast<Args *> (p);
      a->status = run (a->argc, a->argv);
      return nullptr;
    };
  if (pthread_create (&th, &attr, entry, &args) == 0)
    {
      pthread_join (th, nullptr);
      std::cout << std::flush;
      return args.status;
    }
#endif
  return run (argc, argv);
}
