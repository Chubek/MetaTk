// metalib/cpp/File.c — implementation of File.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// File.h and must not be compiled on its own. Mirror: metalib/d/File.d.
#pragma once

namespace metalib
{

namespace file_detail
{
using metaml::Value;

/// "File.read: No such file or directory (`x.txt`)" — errno text from libc,
/// so both dialects report identical messages.
inline Value
fail (const char *prim, const std::string &path)
{
  return support::err (std::string (prim) + ": " + std::strerror (errno) + " (`" + path + "`)");
}

inline bool
read_all (const std::string &path, std::string &out)
{
  int fd = ::open (path.c_str (), O_RDONLY);
  if (fd < 0)
    return false;
  struct stat st;
  if (::fstat (fd, &st) == 0 && S_ISDIR (st.st_mode))
    {
      ::close (fd);
      errno = EISDIR;
      return false;
    }
  char buf[65536];
  while (true)
    {
      ssize_t n = ::read (fd, buf, sizeof buf);
      if (n < 0)
        {
          if (errno == EINTR)
            continue;
          int e = errno;
          ::close (fd);
          errno = e;
          return false;
        }
      if (n == 0)
        break;
      out.append (buf, static_cast<std::size_t> (n));
    }
  ::close (fd);
  return true;
}

inline bool
write_all (const std::string &path, const std::string &data, bool append)
{
  int fd = ::open (path.c_str (), O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
  if (fd < 0)
    return false;
  std::size_t off = 0;
  while (off < data.size ())
    {
      ssize_t n = ::write (fd, data.data () + off, data.size () - off);
      if (n < 0)
        {
          if (errno == EINTR)
            continue;
          int e = errno;
          ::close (fd);
          errno = e;
          return false;
        }
      off += static_cast<std::size_t> (n);
    }
  return ::close (fd) == 0;
}

inline bool
mkdirs (const std::string &path)
{
  if (path.empty ())
    {
      errno = ENOENT;
      return false;
    }
  std::size_t pos = 0;
  while (true)
    {
      pos = path.find ('/', pos + 1);
      std::string part = path.substr (0, pos);
      if (!part.empty () && ::mkdir (part.c_str (), 0755) != 0 && errno != EEXIST)
        return false;
      if (pos == std::string::npos)
        break;
    }
  struct stat st;
  if (::stat (path.c_str (), &st) != 0)
    return false;
  if (!S_ISDIR (st.st_mode))
    {
      errno = ENOTDIR;
      return false;
    }
  return true;
}

inline std::string
strip_slashes (std::string p)
{
  while (p.size () > 1 && p.back () == '/')
    p.pop_back ();
  return p;
}
inline std::string
basename (const std::string &p0)
{
  std::string p = strip_slashes (p0);
  if (p == "/")
    return p;
  auto k = p.rfind ('/');
  return k == std::string::npos ? p : p.substr (k + 1);
}
inline std::string
dirname (const std::string &p0)
{
  std::string p = strip_slashes (p0);
  auto k = p.rfind ('/');
  if (k == std::string::npos)
    return ".";
  if (k == 0)
    return "/";
  return strip_slashes (p.substr (0, k));
}
inline std::string
extension (const std::string &p)
{
  std::string b = basename (p);
  auto k = b.rfind ('.');
  if (k == std::string::npos || k == 0)
    return "";
  return b.substr (k);
}
inline std::string
join (const std::string &a, const std::string &b)
{
  if (a.empty () || (!b.empty () && b[0] == '/'))
    return b;
  if (a.back () == '/')
    return a + b;
  return a + "/" + b;
}
} // namespace file_detail

inline void
install_File (metaml::Session &s)
{
  using namespace support;
  using namespace file_detail;
  prim (s, "File.read", "String -> Result String String", 1, [] (C &, V &a) {
    std::string out;
    if (!read_all (S (a[0]), out))
      return fail ("File.read", S (a[0]));
    return ok (str (out));
  });
  prim (s, "File.read_lines", "String -> Result [String] String", 1, [] (C &, V &a) {
    std::string out;
    if (!read_all (S (a[0]), out))
      return fail ("File.read_lines", S (a[0]));
    std::vector<Value> lines;
    std::size_t b = 0;
    while (b < out.size ())
      {
        auto e = out.find ('\n', b);
        if (e == std::string::npos)
          e = out.size ();
        std::string l = out.substr (b, e - b);
        if (!l.empty () && l.back () == '\r')
          l.pop_back ();
        lines.push_back (str (l));
        b = e + 1;
      }
    return ok (list (lines));
  });
  prim (s, "File.write", "String -> String -> Result Unit String", 2, [] (C &, V &a) {
    if (!write_all (S (a[0]), S (a[1]), false))
      return fail ("File.write", S (a[0]));
    return ok (unit ());
  });
  prim (s, "File.append", "String -> String -> Result Unit String", 2, [] (C &, V &a) {
    if (!write_all (S (a[0]), S (a[1]), true))
      return fail ("File.append", S (a[0]));
    return ok (unit ());
  });
  prim (s, "File.exists", "String -> Bool", 1, [] (C &, V &a) {
    struct stat st;
    return boolean (::stat (S (a[0]).c_str (), &st) == 0);
  });
  prim (s, "File.is_dir", "String -> Bool", 1, [] (C &, V &a) {
    struct stat st;
    return boolean (::stat (S (a[0]).c_str (), &st) == 0 && S_ISDIR (st.st_mode));
  });
  prim (s, "File.is_file", "String -> Bool", 1, [] (C &, V &a) {
    struct stat st;
    return boolean (::stat (S (a[0]).c_str (), &st) == 0 && S_ISREG (st.st_mode));
  });
  prim (s, "File.size", "String -> Result Int String", 1, [] (C &, V &a) {
    struct stat st;
    if (::stat (S (a[0]).c_str (), &st) != 0)
      return fail ("File.size", S (a[0]));
    return ok (integer (static_cast<std::int64_t> (st.st_size)));
  });
  prim (s, "File.remove", "String -> Result Unit String", 1, [] (C &, V &a) {
    struct stat st;
    const char *p = S (a[0]).c_str ();
    if (::lstat (p, &st) != 0)
      return fail ("File.remove", S (a[0]));
    if ((S_ISDIR (st.st_mode) ? ::rmdir (p) : ::unlink (p)) != 0)
      return fail ("File.remove", S (a[0]));
    return ok (unit ());
  });
  prim (s, "File.rename", "String -> String -> Result Unit String", 2, [] (C &, V &a) {
    if (::rename (S (a[0]).c_str (), S (a[1]).c_str ()) != 0)
      return fail ("File.rename", S (a[0]));
    return ok (unit ());
  });
  prim (s, "File.copy", "String -> String -> Result Unit String", 2, [] (C &, V &a) {
    std::string data;
    if (!read_all (S (a[0]), data))
      return fail ("File.copy", S (a[0]));
    if (!write_all (S (a[1]), data, false))
      return fail ("File.copy", S (a[1]));
    return ok (unit ());
  });
  prim (s, "File.mkdir", "String -> Result Unit String", 1, [] (C &, V &a) {
    if (!mkdirs (S (a[0])))
      return fail ("File.mkdir", S (a[0]));
    return ok (unit ());
  });
  prim (s, "File.list_dir", "String -> Result [String] String", 1, [] (C &, V &a) {
    DIR *d = ::opendir (S (a[0]).c_str ());
    if (!d)
      return fail ("File.list_dir", S (a[0]));
    std::vector<std::string> names;
    while (auto *e = ::readdir (d))
      {
        std::string n = e->d_name;
        if (n != "." && n != "..")
          names.push_back (n);
      }
    ::closedir (d);
    std::sort (names.begin (), names.end ());
    return ok (strings (names));
  });
  prim (s, "File.basename", "String -> String", 1, [] (C &, V &a) { return str (basename (S (a[0]))); });
  prim (s, "File.dirname", "String -> String", 1, [] (C &, V &a) { return str (dirname (S (a[0]))); });
  prim (s, "File.extension", "String -> String", 1,
        [] (C &, V &a) { return str (extension (S (a[0]))); });
  prim (s, "File.join", "String -> String -> String", 2,
        [] (C &, V &a) { return str (join (S (a[0]), S (a[1]))); });
  prim (s, "File.temp_dir", "Unit -> String", 1, [] (C &, V &) {
    const char *t = std::getenv ("TMPDIR");
    return str (t && *t ? strip_slashes (t) : std::string ("/tmp"));
  });
}

} // namespace metalib
