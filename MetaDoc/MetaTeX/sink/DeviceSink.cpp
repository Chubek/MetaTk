#include "DeviceSink.hpp"
#include "../utils/Diagnostics.hpp"
#include <stdexcept>
namespace metatex::io {
DeviceSink::DeviceSink(Sink& sink, std::string device) : sink_(sink), device_(std::move(device)) { (void)findDevice(device_); }
void DeviceSink::write(const Document& document) {
    if (util::hasErrors(document)) throw std::runtime_error("cannot publish a document with errors");
    auto bytes = renderDevice(document, device_); sink_.write(bytes); sink_.flush();
}
}
