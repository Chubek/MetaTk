#include "MetaTeX-Device.h"
#include "command/Builtin.hpp"
#include "command/Extern.hpp"
#include "command/MML.hpp"
#include "command/Symbol.hpp"
#include "fonts/FontCatalogue.hpp"
#include "fonts/FontShaping.hpp"
#include "reader/FIFO.hpp"
#include "reader/MSGQueue.hpp"
#include "reader/SharedMemory.hpp"
#include "reader/Socket.hpp"
#include "sink/IPCSinks.hpp"
#include "sink/NetworkSinks.hpp"
#include "sink/DeviceSink.hpp"
#include "sink/DiagSinks.hpp"
#include "utils/Hash.hpp"
#include "utils/Log.hpp"
#include "utils/Memory.hpp"
#include "utils/Posix.hpp"
#include <future>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>
using namespace metatex;
using namespace std::chrono_literals;
#define CHECK(x) do { if (!(x)) throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #x); } while (false)
template<class F> void rejects(F fn) { bool failed = false; try { fn(); } catch (const std::exception&) { failed = true; } CHECK(failed); }
void exact(const Document& doc, const std::string& text) {
    CHECK(doc.diagnostics.empty()); CHECK(doc.blocks.size() == 1); CHECK(doc.blocks[0].spans.size() == 1);
    CHECK(doc.blocks[0].spans[0].kind == SpanKind::Text); CHECK(doc.blocks[0].spans[0].text == text);
    CHECK(renderHTML(doc).find("<p>" + text + "</p>") != std::string::npos);
    CHECK(renderPDF(doc).find("(" + text + ") Tj") != std::string::npos);
}
void commands(const std::filesystem::path& directory) {
    Engine engine; CHECK(command::isBuiltin("def")); CHECK(!command::isBuiltin("notBuiltin"));
    command::registerNative(engine, "join", 2, [](auto args) { return args[0] + args[1]; });
    exact(engine.compile(R"(\join{hello}{world})"), "helloworld");
    command::registerSymbol(engine, "literal", "hello"); exact(engine.compile(R"(\literal)"), "hello");
    command::registerMML(engine, "upper", 1, "String.upper arg1"); exact(engine.compile(R"(\upper{hello})"), "HELLO");
    auto script = directory / "echo.sh"; writeFile(script, "#!/bin/sh\nprintf '%s' \"$1\"\n");
    std::filesystem::permissions(script, std::filesystem::perms::owner_all);
    command::registerExternal(engine, "external", 1, script); exact(engine.compile(R"(\external{hello})"), "hello");
    CHECK(command::runShell("printf '%s' \"$1\"", {"literal;$HOME"}) == "literal;$HOME");
    CHECK(command::runProcess("/bin/printf", {"%s", "hello"}) == "hello");
    rejects([] { command::runShell("exit 7"); });
    rejects([] { command::runShell("sleep 5", {}, {1, 30ms, 64}); });
    rejects([] { command::runShell("printf 123456", {}, {1, 1s, 3}); });
    rejects([&] { command::registerNative(engine, "bad", 10, [](auto) { return ""; }); });
    rejects([&] { command::registerSymbol(engine, "bad", "\xc0\xaf"); });
    command::registerNative(engine, "throwing", 0, [](auto) -> std::string { throw std::runtime_error("callback failure"); });
    auto failed = engine.compile(R"(\throwing)"); CHECK(failed.blocks.empty()); CHECK(failed.diagnostics.size() == 1);
    io::MemorySink memory; io::DiagnosticSink diagnostic(memory); CHECK(diagnostic.write(failed) == 1);
    CHECK(memory.result().find("callback failure") != std::string::npos);
    io::DeviceSink device(memory, "html"); rejects([&] { device.write(failed); });
    auto document = engine.compile("hello"); io::MemorySink output; io::DeviceSink pdf(output, "pdf"); pdf.write(document);
    CHECK(output.result() == renderPDF(document));
}
void streams(const std::filesystem::path& directory) {
    std::string bytes("a\0b\n", 4); bytes += std::string(25000, 'z');
    const auto name = "/metatex-modules-" + std::to_string(::getpid());
    util::ScopeExit cleanup([&] { ::shm_unlink(name.c_str()); ::mq_unlink(name.c_str()); });
    io::SharedMemorySink shared(name); shared.write(bytes); shared.flush(); shared.flush();
    io::SharedMemoryReader reader(name); CHECK(reader.readAll() == bytes);
    rejects([&] { shared.write("after flush"); });
    rejects([&] { io::SharedMemoryReader bounded(name, 2); });
    rejects([&] { io::SharedMemorySink collision(name); collision.flush(); });
    rejects([] { io::SharedMemoryReader invalid("not-posix"); });
    io::MSGQueueSink messages(name, true, 2s, 128);
    auto producer = std::async(std::launch::async, [&] { messages.write(bytes); messages.flush(); messages.finish(); });
    io::MSGQueueReader queue(name, 2s); char first[3]; CHECK(queue.read(first, 3) == 3);
    CHECK(std::string(first, 3) + queue.readAll() == bytes); producer.get();
    rejects([&] { messages.write("after EOF"); });
    io::MSGQueueReader timeout(name, 10ms); rejects([&] { timeout.readAll(); });
    auto fifo = directory / "input.fifo"; CHECK(::mkfifo(fifo.c_str(), 0600) == 0);
    util::ScopeExit unlinkFIFO([&] { std::filesystem::remove(fifo); });
    auto writer = std::async(std::launch::async, [&] { io::FIFOSink sink(fifo); sink.write(bytes); });
    io::FIFOReader fifoReader(fifo); CHECK(fifoReader.readAll() == bytes); writer.get();
    auto brokenFIFO = directory / "broken.fifo"; CHECK(::mkfifo(brokenFIFO.c_str(), 0600) == 0);
    util::ScopeExit unlinkBroken([&] { std::filesystem::remove(brokenFIFO); });
    int temporaryReader = ::open(brokenFIFO.c_str(), O_RDONLY | O_NONBLOCK); CHECK(temporaryReader >= 0);
    io::FIFOSink brokenPipe(brokenFIFO); ::close(temporaryReader);
    rejects([&] { brokenPipe.write("no SIGPIPE"); });
    writeFile(directory / "regular", "test"); rejects([&] { io::FIFOReader wrong(directory / "regular"); });
    int pair[2]; CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    io::SocketReader socketReader(pair[0], true, 2s); io::SocketSink socketWriter(pair[1], true, 2s);
    auto sender = std::async(std::launch::async, [&] { socketWriter.write(bytes); socketWriter.finish(); });
    CHECK(socketReader.readAll() == bytes); sender.get(); rejects([&] { socketWriter.write("late"); });
    int closed[2]; CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, closed) == 0); ::close(closed[0]);
    io::SocketSink broken(closed[1], true, 100ms); rejects([&] { broken.write("no SIGPIPE"); });
    int idle[2]; CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, idle) == 0);
    io::detail::FD other(idle[1]); io::SocketReader idleReader(idle[0], true, 10ms); rejects([&] { idleReader.readAll(); });
    // Real TCP listener exercises DNS/address iteration and nonblocking connect.
    io::detail::FD server(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)); CHECK(server.get() >= 0);
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(::bind(server.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0); CHECK(::listen(server.get(), 1) == 0);
    socklen_t size = sizeof(address); CHECK(::getsockname(server.get(), reinterpret_cast<sockaddr*>(&address), &size) == 0);
    io::SocketSink tcp(io::connectTCP("127.0.0.1", std::to_string(ntohs(address.sin_port)), 2s), true, 2s);
    int accepted = ::accept(server.get(), nullptr, nullptr); CHECK(accepted >= 0);
    io::SocketReader tcpReader(accepted, true, 2s); tcp.write("tcp"); tcp.finish(); CHECK(tcpReader.readAll() == "tcp");
    auto unixPath = directory / "socket"; sockaddr_un local{}; local.sun_family = AF_UNIX;
    CHECK(unixPath.string().size() < sizeof(local.sun_path)); std::strcpy(local.sun_path, unixPath.c_str());
    io::detail::FD unixServer(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    CHECK(::bind(unixServer.get(), reinterpret_cast<sockaddr*>(&local), sizeof(local)) == 0); CHECK(::listen(unixServer.get(), 1) == 0);
    util::ScopeExit unlinkSocket([&] { std::filesystem::remove(unixPath); });
    io::SocketSink unixWriter(io::connectUnix(unixPath, 2s), true, 2s);
    io::SocketReader unixReader(::accept(unixServer.get(), nullptr, nullptr), true, 2s);
    unixWriter.write("unix"); unixWriter.finish(); CHECK(unixReader.readAll() == "unix");
}
void fontTests(const std::filesystem::path& path) {
    fonts::FontLoader loader; auto font = loader.load(path); CHECK(font->metadata.unitsPerEm > 0);
    CHECK(loader.load(path) == font); CHECK(!font->metadata.family.empty());
    auto manifest = fonts::FontManifest::parse("metatex-fonts 1\n\"regular\" \"" + path.string() + "\" 0\n");
    CHECK(fonts::FontManifest::parse(manifest.serialize()).entries.size() == 1);
    fonts::FontCatalogue catalogue; catalogue.import(manifest, loader); CHECK(catalogue.find("regular") == font);
    CHECK(catalogue.match(font->metadata.family) == font); rejects([&] { catalogue.import(manifest, loader); });
    CHECK(catalogue.ids() == std::vector<std::string>{"regular"});
    rejects([] { fonts::FontManifest::parse("metatex-fonts 2"); });
    rejects([] { fonts::inspectFont(std::vector<std::uint8_t>{1, 2, 3}); });
    auto run = fonts::shape(*font, "office"); CHECK(!run.glyphs.empty()); CHECK(run.xAdvance > 0); CHECK(!run.missingGlyphs);
    fonts::ShapeOptions large; large.pointSize = 24; auto doubled = fonts::shape(*font, "office", large);
    CHECK(std::abs(doubled.xAdvance - run.xAdvance * 2) < 1e-8);
    fonts::ShapeOptions noLiga; noLiga.features = {"liga=0"}; CHECK(fonts::shape(*font, "office", noLiga).glyphs.size() >= run.glyphs.size());
    auto rtl = fonts::shape(*font, "سلام"); CHECK(rtl.direction == fonts::Direction::RightToLeft); CHECK(!rtl.glyphs.empty());
    CHECK(rtl.glyphs.front().cluster >= rtl.glyphs.back().cluster);
    rejects([&] { fonts::shape(*font, "\xff"); });
    fonts::ShapeOptions invalid; invalid.pointSize = -1; rejects([&] { fonts::shape(*font, "a", invalid); });
    loader.clear(); CHECK(loader.load(path) != font);
}
int main(int argc, char** argv) {
    try {
        CHECK(argc == 3); std::filesystem::path directory = argv[1]; std::filesystem::create_directories(directory);
        commands(directory); streams(directory); fontTests(argv[2]);
        CHECK(util::crc32("123456789") == 0xcbf43926); CHECK(util::hash64("") == 14695981039346656037ULL);
        rejects([] { util::checkedSize(std::numeric_limits<std::size_t>::max(), 2); });
        std::ostringstream log; util::Logger logger(log); logger.log(util::LogLevel::Debug, "hidden"); logger.log(util::LogLevel::Error, "bad\nline");
        CHECK(log.str() == "error: bad\\x0aline\n");
        std::cout << "Module integration tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
