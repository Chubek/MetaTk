// MAMMAL project descriptions are ordinary MetaML programs, not a second DSL.
#include "../MetaML.hpp"
#include <fstream>
#include <iostream>
#include <set>
#if !defined(_WIN32)
#include <spawn.h>
#include <sys/wait.h>
#include <cerrno>
extern char **environ;
#endif

namespace
{
namespace fs = std::filesystem;
struct Copy { std::string source, destination; };
struct Project
{
  fs::path root;
  std::string name, main, build_dir = "_mammal";
  std::vector<std::string> files, directories, paths, tests;
  std::vector<Copy> fixtures, artifacts, installs;
};

// Resolve symlinks too: neither input nor output paths may escape their root.
fs::path within (const fs::path &root, const std::string &relative, bool allow_root = false)
{
  fs::path p (relative);
  if (p.empty () || p.is_absolute ()) throw std::runtime_error ("expected a relative path: " + relative);
  auto base = fs::weakly_canonical (root);
  auto full = fs::weakly_canonical (base / p);
  auto rel = full.lexically_relative (base);
  if (rel.empty () || (!allow_root && rel == ".") || *rel.begin () == "..")
    throw std::runtime_error ("path escapes or replaces its root: " + relative);
  return full;
}

bool source_file (const fs::path &p)
{
  auto ext = p.extension ();
  return ext == ".mml" || ext == ".mmli" || ext == ".mlli";
}

void load (Project &p, const fs::path &file)
{
  metaml::Session session;
  session.allow_all_primitives ();
  auto setting = [&] (const std::string &name, auto fn)
    {
      session.register_primitive ("Mammal." + name, "String -> Unit", 1,
          [fn] (metaml::PrimContext &, std::vector<metaml::Value> &a)
            { fn (metaml::str_of (a[0])); return metaml::Value::unit (); });
    };
  setting ("project", [&] (auto s) { p.name = s; });
  setting ("main", [&] (auto s) { p.main = s; });
  setting ("build_dir", [&] (auto s) { p.build_dir = s; });
  setting ("file", [&] (auto s) { p.files.push_back (s); });
  setting ("directory", [&] (auto s) { p.directories.push_back (s); });
  setting ("module_path", [&] (auto s) { p.paths.push_back (s); });
  setting ("test", [&] (auto s) { p.tests.push_back (s); });
  auto copying = [&] (const std::string &name, std::vector<Copy> &copies)
    {
      session.register_primitive ("Mammal." + name, "String -> String -> Unit", 2,
          [&copies] (metaml::PrimContext &, std::vector<metaml::Value> &a)
            {
              copies.push_back ({ metaml::str_of (a[0]), metaml::str_of (a[1]) });
              return metaml::Value::unit ();
            });
    };
  copying ("fixture", p.fixtures);
  copying ("artifact", p.artifacts);
  copying ("install", p.installs);
  auto r = session.run_file (file.string (), metaml::RunMode::Quiet);
  for (const auto &d : r.errors) std::cerr << session.render (d);
  if (!r.ok ()) throw std::runtime_error ("invalid MAMMAL project");
  if (p.name.empty ()) throw std::runtime_error ("MAMMAL must call Mammal.project");
}

int invoke (const std::string &binary, const std::vector<std::string> &args)
{
#if defined(_WIN32)
  throw std::runtime_error ("process launching is not yet supported on Windows");
#else
  std::vector<char *> argv{ const_cast<char *> (binary.c_str ()) };
  for (const auto &a : args) argv.push_back (const_cast<char *> (a.c_str ()));
  argv.push_back (nullptr);
  pid_t pid;
  int error = posix_spawnp (&pid, binary.c_str (), nullptr, nullptr, argv.data (), environ);
  if (error) throw std::runtime_error ("cannot launch " + binary + ": " + std::strerror (error));
  int status;
  while (waitpid (pid, &status, 0) < 0)
    if (errno != EINTR) throw std::runtime_error ("cannot wait for metaml");
  return WIFEXITED (status) ? WEXITSTATUS (status) : 1;
#endif
}

void copy_input (const fs::path &source, const fs::path &destination)
{
  if (!fs::is_regular_file (source)) throw std::runtime_error ("not a regular file: " + source.string ());
  fs::create_directories (destination.parent_path ());
  fs::copy_file (source, destination, fs::copy_options::overwrite_existing);
}

const char help[] = R"(Usage: metaml-mammal [OPTIONS] COMMAND [-- PROGRAM_ARGS...]
Commands: init, show, check, build, run, test, install
  -C DIR          Discover MAMMAL starting at DIR (searches parents)
  --metaml FILE   MetaML executable (default: sibling metaml, then PATH)
  --prefix DIR    Installation prefix (default: /usr/local)
  --destdir DIR   Stage installation under DIR
  -h, --help      Show this help
MAMMAL is evaluated as MetaML with Mammal.project/file/directory/main/test,
Mammal.module_path/build_dir, and Mammal.fixture/artifact/install primitives.
)";

int run (int argc, char **argv)
{
  auto program_path = fs::absolute (argv[0]);
  fs::path start = fs::current_path (), prefix = "/usr/local", destdir;
  std::string command, binary;
  std::vector<std::string> program_args;
  for (int i = 1; i < argc; ++i)
    {
      std::string a = argv[i];
      auto need = [&] () -> std::string
        { if (++i == argc) throw std::invalid_argument ("missing argument for " + a); return argv[i]; };
      if (a == "-h" || a == "--help") { std::cout << help; return 0; }
      if (a == "-C") start = fs::absolute (need ());
      else if (a == "--prefix") prefix = fs::absolute (need ());
      else if (a == "--destdir") destdir = fs::absolute (need ());
      else if (a == "--metaml") binary = fs::absolute (need ()).string ();
      else if (a == "--") { while (++i < argc) program_args.emplace_back (argv[i]); break; }
      else if (a.starts_with ("-") || !command.empty ()) throw std::invalid_argument ("unexpected argument " + a);
      else command = a;
    }
  const std::set<std::string> commands{ "init", "show", "check", "build", "run", "test", "install" };
  if (!commands.count (command)) throw std::invalid_argument ("expected a command; use --help");
  if (!program_args.empty () && command != "run") throw std::invalid_argument ("program arguments require run");
  if (command == "init")
    {
      if (!fs::is_directory (start)) throw std::runtime_error ("project directory does not exist");
      if (fs::exists (start / "MAMMAL")) throw std::runtime_error ("MAMMAL already exists");
      std::ofstream out (start / "MAMMAL");
      out << "Mammal.project(" << metaml::detail::quote_string (start.filename ().string ()) << ")\n"
          << "Mammal.main(\"main.mml\")\nMammal.file(\"main.mml\")\n";
      if (!out) throw std::runtime_error ("cannot write MAMMAL");
      return 0;
    }
  while (!fs::is_regular_file (start / "MAMMAL"))
    { auto parent = start.parent_path (); if (parent == start) throw std::runtime_error ("cannot find MAMMAL"); start = parent; }
  Project p;
  p.root = fs::canonical (start);
  // Relative imports in MAMMAL use its directory; project paths always use root.
  fs::current_path (p.root);
  load (p, p.root / "MAMMAL");
  auto build = within (p.root, p.build_dir);
  for (const auto &dir : p.directories)
    {
      auto path = within (p.root, dir, true);
      if (!fs::is_directory (path)) throw std::runtime_error ("not a directory: " + dir);
      std::vector<std::string> found;
      for (const auto &entry : fs::recursive_directory_iterator (path))
        if (entry.is_regular_file () && source_file (entry.path ()))
          {
            auto canonical = within (p.root, entry.path ().lexically_relative (p.root).string ());
            if (canonical == build || canonical.string ().starts_with (build.string () + "/")) continue;
            found.push_back (entry.path ().lexically_relative (p.root).generic_string ());
          }
      std::sort (found.begin (), found.end ());
      p.files.insert (p.files.end (), found.begin (), found.end ());
    }
  if (!p.main.empty ()) p.files.push_back (p.main);
  p.files.insert (p.files.end (), p.tests.begin (), p.tests.end ());
  std::set<std::string> files;
  for (const auto &f : p.files)
    {
      auto path = within (p.root, f);
      if (!source_file (path) || !fs::is_regular_file (path)) throw std::runtime_error ("invalid source: " + f);
      files.insert (path.lexically_relative (p.root).generic_string ());
      if (path.extension () == ".mml")
        for (auto ext : { ".mmli", ".mlli" })
          { auto iface = path; iface.replace_extension (ext); if (fs::is_regular_file (iface)) files.insert (iface.lexically_relative (p.root).generic_string ()); }
    }
  for (const auto &path : p.paths)
    if (!fs::is_directory (within (p.root, path, true))) throw std::runtime_error ("invalid module path: " + path);
  // Plan and validate the complete distribution, even for show/check.
  std::vector<std::pair<fs::path, fs::path>> copies;
  std::set<fs::path> outputs;
  auto stage = [&] (const fs::path &src, const std::string &dst)
    {
      auto target = within (build, dst);
      if (!fs::is_regular_file (src)) throw std::runtime_error ("missing input: " + src.string ());
      if (src.string ().starts_with (build.string () + "/")) throw std::runtime_error ("build directory contains an input: " + src.string ());
      if (src == target || !outputs.insert (target).second) throw std::runtime_error ("conflicting build destination: " + dst);
      copies.emplace_back (src, target);
    };
  for (const auto &f : files) stage (within (p.root, f), f);
  for (const auto &f : p.fixtures) stage (within (p.root, f.source), f.destination);
  for (const auto &a : p.artifacts) stage (within (p.root, a.source), a.destination);
  auto install_root = destdir.empty () ? prefix : destdir / prefix.relative_path ();
  std::vector<std::pair<fs::path, fs::path>> installs;
  std::set<fs::path> destinations;
  for (const auto &i : p.installs)
    {
      auto src = within (build, i.source), dst = within (install_root, i.destination);
      if (!outputs.count (src) || !destinations.insert (dst).second) throw std::runtime_error ("invalid installation: " + i.destination);
      installs.emplace_back (src, dst);
    }
  if (command == "show")
    {
      std::cout << "project " << p.name << "\nroot " << p.root.string () << "\nbuild " << build.string () << '\n';
      for (const auto &f : files) std::cout << "file " << f << '\n';
      for (const auto &f : p.fixtures) std::cout << "fixture " << f.source << " -> " << f.destination << '\n';
      for (const auto &a : p.artifacts) std::cout << "artifact " << a.source << " -> " << a.destination << '\n';
      for (const auto &i : p.installs) std::cout << "install " << i.source << " -> " << i.destination << '\n';
      return 0;
    }
  if (binary.empty ())
    {
      std::error_code ec;
      auto executable = fs::read_symlink ("/proc/self/exe", ec);
      auto sibling = (ec ? program_path : executable).parent_path () / "metaml";
      binary = fs::is_regular_file (sibling) ? sibling.string () : "metaml";
    }
  std::vector<std::string> common{ "--no-init", "--no-history", "-I", p.root.string () };
  for (const auto &path : p.paths) { common.push_back ("-I"); common.push_back (within (p.root, path, true).string ()); }
  for (const auto &f : files)
    {
      auto args = common;
      args.insert (args.end (), { "--check", within (p.root, f).string () });
      if (int status = invoke (binary, args)) return status;
    }
  if (command == "check") return 0;
  if (command == "run" && p.main.empty ()) throw std::runtime_error ("MAMMAL has no Mammal.main");
  fs::create_directories (build);
  for (const auto &[src, dst] : copies) copy_input (src, dst);
  if (command == "build") return 0;
  if (command == "install")
    {
      for (const auto &[src, dst] : installs) copy_input (src, dst);
      return 0;
    }
  fs::current_path (build);
  auto execute = [&] (const std::string &file, bool arguments)
    {
      std::vector<std::string> args{ "--no-init", "--no-history", "-I", build.string () };
      for (const auto &path : p.paths) { args.push_back ("-I"); args.push_back (within (build, path, true).string ()); }
      args.push_back (within (build, file).string ());
      if (arguments) { args.push_back ("--"); args.insert (args.end (), program_args.begin (), program_args.end ()); }
      return invoke (binary, args);
    };
  if (command == "run")
    return execute (p.main, true);
  for (const auto &test : p.tests) if (int status = execute (test, false)) return status;
  return 0;
}
} // namespace

int main (int argc, char **argv)
{
  try { return run (argc, argv); }
  catch (const std::invalid_argument &e) { std::cerr << "metaml-mammal: " << e.what () << '\n'; return 2; }
  catch (const std::exception &e) { std::cerr << "metaml-mammal: " << e.what () << '\n'; return 1; }
}
