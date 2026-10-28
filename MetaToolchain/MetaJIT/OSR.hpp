#ifndef METAJIT_OSR_HPP
#define METAJIT_OSR_HPP
#include "Deopt.hpp"
#include <limits>
#include <stdexcept>

namespace metajit {
using Transition = std::int64_t (*)(NativeFrame *);
/** Enter a compiled trace at an exact VM instruction boundary. Native code
 * writes through the existing slots and returns a side-exit code. Commit the
 * materialized PC/status/budget to the interpreter without replaying any
 * executed instruction. Throws on invalid entry state or broken compiler ABI;
 * only trusted, validated compiler output may be passed as a transition. */
inline NativeExit osr_enter(const metavm::Program &program, metavm::Frame &frame,
                           std::uint64_t header, std::uint64_t budget, Transition entry) {
    metavm::validate_frame(program,frame);
    if (!entry || frame.status!=metavm::Status::Running || frame.pc!=header)
        throw std::invalid_argument("MetaJIT: invalid OSR entry state");
    if (budget>std::numeric_limits<std::uint64_t>::max()-frame.steps)
        throw std::length_error("MetaJIT: step counter overflow");
    NativeFrame native{frame.values.data(),frame.pc,budget,static_cast<std::uint64_t>(frame.status),
        static_cast<std::uint64_t>(frame.trap),frame.result};
    frame.active=true;
    struct Reset { bool &active;~Reset() { active=false; } } reset{frame.active};
    auto reason=static_cast<NativeExit>(entry(&native));
    if (native.values!=frame.values.data() || native.pc>=program.code().size() || native.remaining>budget ||
        native.status>static_cast<std::uint64_t>(metavm::Status::Trapped) ||
        native.trap>static_cast<std::uint64_t>(metavm::Trap::HostFailure) ||
        ((native.status==static_cast<std::uint64_t>(metavm::Status::Trapped))!=(native.trap!=0)) ||
        reason<NativeExit::Boundary || reason>NativeExit::Trapped ||
        ((reason==NativeExit::Returned)!=(native.status==static_cast<std::uint64_t>(metavm::Status::Returned))) ||
        ((reason==NativeExit::Trapped)!=(native.status==static_cast<std::uint64_t>(metavm::Status::Trapped))) ||
        (reason==NativeExit::Budget && native.remaining!=0))
        throw std::runtime_error("MetaJIT: corrupt native side-exit ABI");
    frame.pc=native.pc;frame.steps+=budget-native.remaining;
    frame.status=static_cast<metavm::Status>(native.status);frame.trap=static_cast<metavm::Trap>(native.trap);
    frame.result=native.result;
    return reason;
}
} // namespace metajit
#endif
