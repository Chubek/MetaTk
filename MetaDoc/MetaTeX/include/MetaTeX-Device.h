#pragma once
#include "MetaTeX.h"
#include <span>
namespace metatex {
std::string renderPostScript(const Document& document);
std::string renderTroffIR(const Document& document);
std::string renderEPUB(const Document& document);
struct DeviceInfo { std::string_view name, extension, mediaType; bool binary; };
std::span<const DeviceInfo> devices();
const DeviceInfo& findDevice(std::string_view name);
std::string renderDevice(const Document& document, std::string_view name);
}
