#include "MetaTeX.h"
#include "MetaTeX-Reader.h"
#include "MetaTeX-Sink.h"
namespace metatex {
std::string readFile(const std::filesystem::path& path) {
    return readFile(path, 16u * 1024u * 1024u);
}
std::string readFile(const std::filesystem::path& path, std::size_t maxBytes) {
    return io::FileReader(path).readAll(maxBytes);
}
void writeFile(const std::filesystem::path& path, std::string_view bytes) {
    io::FileSink output(path);
    output.write(bytes);
    output.flush();
}
}
