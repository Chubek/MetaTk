#ifndef METAJIT_DEOPT_HPP
#define METAJIT_DEOPT_HPP
#include <vmspec/VM.hpp>
#include <stdexcept>
#include <utility>

namespace metajit {
enum class NativeExit : std::int64_t { Boundary, Guard, Budget, Returned, Trapped };
/** Version-1 transition ABI. All VM slots are materialized in values at every
 * safepoint; there are no hidden live native registers or stack values. pc is
 * the next VM instruction, except returns/traps retain their instruction PC.
 * Generated code may only change slot contents and the scalar fields below. */
struct NativeFrame {
    static constexpr std::uint32_t abi_version = 1;
    std::int64_t *values;
    std::uint64_t pc, remaining, status, trap;
    std::int64_t result;
};
/** Owning, portable deoptimization snapshot, including all live values. Copies
 * may outlive the native executable. Must be restored against the same Program;
 * caller owns program identity and must synchronize frame mutations. */
class DeoptSnapshot {
public:
    DeoptSnapshot(const metavm::Program &program, const metavm::Frame &frame, NativeExit reason)
        : frame_(frame), reason_(reason) {
        metavm::validate_frame(program,frame_);
        if (reason<NativeExit::Boundary || reason>NativeExit::Trapped ||
            ((reason==NativeExit::Returned)!=(frame.status==metavm::Status::Returned)) ||
            ((reason==NativeExit::Trapped)!=(frame.status==metavm::Status::Trapped)))
            throw std::invalid_argument("MetaJIT: invalid deoptimization reason/state");
    }
    void restore(const metavm::Program &program, metavm::Frame &out) const {
        metavm::validate_frame(program,frame_);
        if (out.active) throw std::invalid_argument("MetaJIT: cannot restore an active frame");
        auto copy=frame_;out=std::move(copy);
    }
    const metavm::Frame &frame() const noexcept { return frame_; }
    NativeExit reason() const noexcept { return reason_; }
private:
    metavm::Frame frame_;
    NativeExit reason_;
};
} // namespace metajit
#endif
