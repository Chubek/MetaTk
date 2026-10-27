#include "MetaTeX-Reader.h"
#include "MetaTeX-Sink.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto path = std::filesystem::path(argv[1]);
    std::string payload(100000, 'X');
    payload.append("\0\x01\xFF", 3);
    { metatex::io::FileSink sink(path); sink.write(payload); sink.flush(); }
    { metatex::io::FileReader reader(path); assert(reader.readAll(100003) == payload); }
    { metatex::io::MappedFileReader reader(path); assert(reader.readAll(100003) == payload); }
    { metatex::io::MemoryReader reader(payload); assert(reader.readAll() == payload); }
    { metatex::io::MemoryReader reader(payload); bool threw=false; try { reader.readAll(1024); } catch(const std::runtime_error&) { threw=true; } assert(threw); }
    { std::istringstream stream(payload); metatex::io::StreamReader reader(stream); assert(reader.readAll() == payload); }
    int fds[2]; assert(pipe(fds) == 0);
    { metatex::io::DescriptorSink sink(fds[1], true); sink.write("pipe-test"); }
    { metatex::io::DescriptorReader source(fds[0], true); assert(source.readAll() == "pipe-test"); }
    { metatex::io::MemorySink sink; sink.write(payload); assert(sink.take() == payload); }
    std::filesystem::remove(path);
}
