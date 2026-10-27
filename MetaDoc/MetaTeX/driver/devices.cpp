#include "MetaTeX-Device.h"
#include <array>
#include <stdexcept>
namespace metatex {
std::span<const DeviceInfo> devices() {
    static constexpr std::array<DeviceInfo, 7> list{{
        {"html", "html", "text/html", false}, {"pdf", "pdf", "application/pdf", true},
        {"text", "txt", "text/plain", false}, {"json", "json", "application/json", false},
        {"ps", "ps", "application/postscript", false}, {"troff-ir", "dit", "application/x-troff", false},
        {"epub", "epub", "application/epub+zip", true}}};
    return list;
}
const DeviceInfo& findDevice(std::string_view name) {
    for (const auto& device : devices()) if (device.name == name) return device;
    throw std::invalid_argument("unsupported device: " + std::string(name));
}
std::string renderDevice(const Document& document, std::string_view name) {
    (void)findDevice(name);
    if (name == "html") return renderHTML(document);
    if (name == "pdf") return renderPDF(document);
    if (name == "json") return renderJSON(document);
    if (name == "text") return renderText(document);
    if (name == "ps") return renderPostScript(document);
    if (name == "troff-ir") return renderTroffIR(document);
    return renderEPUB(document);
}
}
