#pragma once
#include "MetaTeX-Device.h"
#include "MetaTeX-Sink.h"
namespace metatex::io {
class DeviceSink {
public:
    DeviceSink(Sink& sink, std::string device);
    void write(const Document& document);
private:
    Sink& sink_;
    std::string device_;
};
}
