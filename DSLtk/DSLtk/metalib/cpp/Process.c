// metalib/cpp/Process.c — implementation of Process.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Process.h and must not be compiled on its own. Mirror: metalib/d/Process.d.
#pragma once

namespace metalib
{

namespace process_detail
{
using metaml::Value;

struct Captured
{
  int status = -1;
  std::string out, err;
  std::string error;
};

/// Runs argv[0] (PATH lookup) with stdin from /dev/null, capturing stdout
/// and stderr (POSIX pipes + posix_spawnp + poll).
inline Captured
run (const std::vector<std::string> &argv, bool capture_err)
{
  Captured r;
  int po[2], pe[2] = { -1, -1 };
  if (::pipe (po) != 0 || (capture_err && ::pipe (pe) != 0))
    {
      r.error = std::strerror (errno);
      return r;
    }
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init (&fa);
  posix_spawn_file_actions_addopen (&fa, 0, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_adddup2 (&fa, po[1], 1);
  posix_spawn_file_actions_addclose (&fa, po[0]);
  posix_spawn_file_actions_addclose (&fa, po[1]);
  if (capture_err)
    {
      posix_spawn_file_actions_adddup2 (&fa, pe[1], 2);
      posix_spawn_file_actions_addclose (&fa, pe[0]);
      posix_spawn_file_actions_addclose (&fa, pe[1]);
    }
  std::vector<char *> args;
  for (auto &a : argv)
    args.push_back (const_cast<char *> (a.c_str ()));
  args.push_back (nullptr);
  pid_t pid;
  int rc = posix_spawnp (&pid, args[0], &fa, nullptr, args.data (), environ);
  posix_spawn_file_actions_destroy (&fa);
  ::close (po[1]);
  if (capture_err)
    ::close (pe[1]);
  if (rc != 0)
    {
      ::close (po[0]);
      if (capture_err)
        ::close (pe[0]);
      r.error = std::strerror (rc);
      return r;
    }
  struct pollfd fds[2] = { { po[0], POLLIN, 0 }, { pe[0], POLLIN, 0 } };
  int open_fds = capture_err ? 2 : 1;
  char buf[65536];
  while (open_fds > 0)
    {
      if (::poll (fds, capture_err ? 2 : 1, -1) < 0)
        {
          if (errno == EINTR)
            continue;
          break;
        }
      for (int k = 0; k < (capture_err ? 2 : 1); ++k)
        if (fds[k].fd >= 0 && (fds[k].revents & (POLLIN | POLLHUP | POLLERR)))
          {
            ssize_t n = ::read (fds[k].fd, buf, sizeof buf);
            if (n > 0)
              (k == 0 ? r.out : r.err).append (buf, static_cast<std::size_t> (n));
            else if (n == 0 || errno != EINTR)
              {
                ::close (fds[k].fd);
                fds[k].fd = -1;
                --open_fds;
              }
          }
    }
  int st = 0;
  while (::waitpid (pid, &st, 0) < 0 && errno == EINTR)
    ;
  r.status = WIFEXITED (st) ? WEXITSTATUS (st) : 128 + (WIFSIGNALED (st) ? WTERMSIG (st) : 0);
  return r;
}
} // namespace process_detail

inline void
install_Process (metaml::Session &s)
{
  using namespace support;
  using namespace process_detail;
  prim (s, "Process.run", "String -> [String] -> Result (Int, String, String) String", 2,
        [] (C &, V &a) {
          std::vector<std::string> argv{ S (a[0]) };
          for (auto &x : items (a[1]))
            argv.push_back (S (x));
          auto r = run (argv, true);
          if (!r.error.empty ())
            return err ("Process.run: cannot run `" + S (a[0]) + "`: " + r.error);
          return ok (triple (integer (r.status), str (r.out), str (r.err)));
        });
  prim (s, "Process.shell", "String -> Result (Int, String) String", 1, [] (C &c, V &a) {
    c.session.flush_output ();
    auto r = run ({ "/bin/sh", "-c", S (a[0]) }, false);
    if (!r.error.empty ())
      return err ("Process.shell: " + r.error);
    return ok (pair (integer (r.status), str (r.out)));
  });
  prim (s, "Process.env", "String -> Option String", 1, [] (C &, V &a) {
    const char *v = std::getenv (S (a[0]).c_str ());
    return v ? some (str (v)) : none ();
  });
  prim (s, "Process.set_env", "String -> String -> Unit", 2, [] (C &, V &a) {
    ::setenv (S (a[0]).c_str (), S (a[1]).c_str (), 1);
    return unit ();
  });
  prim (s, "Process.args", "Unit -> [String]", 1, [] (C &c, V &) { return strings (c.session.args); });
  prim (s, "Process.cwd", "Unit -> String", 1, [] (C &, V &) {
    char buf[4096];
    return str (::getcwd (buf, sizeof buf) ? buf : "");
  });
  prim (s, "Process.chdir", "String -> Result Unit String", 1, [] (C &, V &a) {
    if (::chdir (S (a[0]).c_str ()) != 0)
      return err (std::string ("Process.chdir: ") + std::strerror (errno) + " (`" + S (a[0]) + "`)");
    return ok (unit ());
  });
  prim (s, "Process.pid", "Unit -> Int", 1,
        [] (C &, V &) { return integer (static_cast<std::int64_t> (::getpid ())); });
  prim (s, "Process.clock_ms", "Unit -> Int", 1, [] (C &, V &) {
    struct timespec ts;
    ::clock_gettime (CLOCK_MONOTONIC, &ts);
    return integer (static_cast<std::int64_t> (ts.tv_sec) * 1000 + ts.tv_nsec / 1000000);
  });
  prim (s, "Process.time", "Unit -> Float", 1, [] (C &, V &) {
    struct timespec ts;
    ::clock_gettime (CLOCK_REALTIME, &ts);
    return floating (static_cast<double> (ts.tv_sec) + static_cast<double> (ts.tv_nsec) / 1e9);
  });
  prim (s, "Process.exit", "Int -> 'a", 1, [] (C &c, V &a) -> Value {
    c.session.flush_output ();
    std::fflush (nullptr);
    std::_Exit (static_cast<int> (a[0].i));
  });
}

} // namespace metalib
