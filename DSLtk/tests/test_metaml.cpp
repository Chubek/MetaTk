#include "MetaML.hpp"
#include <fstream>
#include <chrono>
#include <stdexcept>

static void require(bool b) { if (!b) throw std::runtime_error("MetaML regression"); }

int main(int argc, char **argv)
{
  require(argc == 2);
  for (auto source : {"$999999999999999999999999", "(#999999999999999999999999 + 1)"})
    require(!metaml::lex(source).diagnostics.empty());
  for (auto text : {"", "_", "1_", "1__2", "0x_1"})
    require(!metaml::detail::parse_int(text));
  for (auto text : {"", "_", "\v1", "1_", "1__2"})
    require(!metaml::detail::parse_float(text));
  require(metaml::detail::parse_int("1_000") == 1000);
  require(metaml::detail::parse_float("1_000.5") == 1000.5);
  for (auto source : {"1_", "1.0__2", "1__0"})
    require(!metaml::lex(source).diagnostics.empty());
  metaml::Session s;
  require(s.load_stdlib(argv[1]).is_ok());
  require(s.eval("List.range (~9223372036854775808) 9223372036854775807").is_err());
  require(s.eval("[Int.min_value .. Int.max_value]").is_err());
  require(metaml::to_string(s.eval("[Int.max_value .. Int.max_value]")) == "[9223372036854775807]");
  require(s.eval("String.repeat 9223372036854775807 \"\"").is_ok());
  require(s.eval("String.repeat 9223372036854775807 \"xx\"").is_err());
  s.register_primitive("Host.boom", [] (std::int64_t) -> std::int64_t {
    throw std::runtime_error("host failure");
  });
  auto bad = s.eval("Host.boom 0");
  require(bad.is_err() && bad.unwrap_err()[0].code == "M0409");
  require(metaml::to_string(s.eval("1 + 2")) == "3");
  s.register_primitive("Host.inc", [](std::int64_t n) { return n + 1; });
  require(s.eval("saved := Host.inc").is_ok());
  s.register_primitive("Host.inc", [](std::int64_t n) { return n + 2; });
  require(metaml::to_string(s.eval("saved 1")) == "2");
  require(metaml::to_string(s.eval("Host.inc 1")) == "3");
  // Run the extensions without the optional host bundle as well.
  s.set_output([](std::string_view) {});
  require(s.load_file((std::filesystem::path(argv[1]).parent_path()
                       / "tests/integration/extensions.ml").string()).is_ok());
  auto temporary = std::filesystem::is_directory("/tmp/opencode")
      ? std::filesystem::path("/tmp/opencode") : std::filesystem::temp_directory_path();
  const auto file = temporary / ("metaml-retry-cpp-" + std::to_string(
      std::chrono::high_resolution_clock::now().time_since_epoch().count()) + ".ml");
  { std::ofstream out(file); out << "val retry := missing_name\n"; }
  const auto source = "import " + metaml::detail::quote_string(file.string());
  require(s.eval(source).is_err());
  { std::ofstream out(file); out << "val retry := 42\n"; }
  require(s.eval(source).is_ok());
  require(metaml::to_string(s.eval("retry")) == "42");
  std::filesystem::remove(file);
}
