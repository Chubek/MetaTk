#include "Scripting.h"
#include <MetaML.hpp>
#include <metalib/cpp/Metalib.h>
#include <metalib/cpp/File.h>
#include <metalib/cpp/Process.h>
#include <metalib/cpp/CBridge.h>
#include <metalib/cpp/FFI.h>
#include <metalib/cpp/HTTP.h>
#include <metalib/cpp/Threading.h>
#include <metalib/cpp/Channels.h>
#include <metalib/cpp/Fiber.h>
#include <metalib/msgpack/MsgPack.hpp>
#include <metalib/sqlite/SQLite.hpp>
#include <metalib/nng/NNG.hpp>
#include <metalib/sysipc/IPC.hpp>
#include <metalib/webserver/Webserver.hpp>

namespace metatex::detail {
namespace {
std::string diagnostics(metaml::Session& session,const std::vector<metaml::Diagnostic>& errors){
    std::string result;
    for(const auto& error:errors)result+=session.render(error);
    return result;
}
}
std::string runMetaML(std::string_view source,const std::vector<std::string>& arguments,const std::filesystem::path& filename){
    std::string output;
    std::mutex outputMutex;
    auto append=[&](std::string_view text){
        std::lock_guard lock(outputMutex);
        if(text.size()>4*1024*1024-output.size())throw std::runtime_error("MetaML output limit exceeded");
        output.append(text);
    };
    // Each invocation owns its interpreter and all native resources. Output is
    // buffered until successful completion, before the engine sees any tokens.
    {
    metaml::Session session;
    session.set_output(append);
    session.set_max_depth(256);
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    session.interp().cancelled=[deadline]{return std::chrono::steady_clock::now()>=deadline;};
    metalib::install_all(session);
    metalib::install_File(session);
    metalib::install_Process(session);
    metalib::install_CBridge(session);
    metalib::install_FFI(session);
    metalib::install_HTTP(session);
    metalib::install_Threading(session);
    metalib::install_Channels(session);
    metalib::install_Fiber(session);
    metalib::install_MsgPack(session);
    metalib::install_SQLite(session);
    metalib::install_NNG(session);
    metalib::install_IPC(session);
    metalib::install_Webserver(session);
    session.allow_all_primitives();
    session.args=arguments;
    auto loaded=session.load_stdlib();
    if(loaded.is_err())throw std::runtime_error(diagnostics(session,loaded.unwrap_err()));
    for(std::size_t i=0;i<arguments.size();++i){
        auto bound=session.eval("val arg"+std::to_string(i+1)+" := "+metaml::detail::quote_string(arguments[i]));
        if(bound.is_err())throw std::runtime_error(diagnostics(session,bound.unwrap_err()));
    }
    auto result=session.eval(source,filename.string());
    if(result.is_err())throw std::runtime_error(diagnostics(session,result.unwrap_err()));
    const auto& value=result.unwrap();
    if(value.kind==metaml::VK::Str)append(metaml::str_of(value));
    else if(value.kind!=metaml::VK::Unit)append(metaml::to_string(value));
    session.close();
    }
    return output;
}
}
