#include "IPCSinks.hpp"
#include "../utils/Memory.hpp"
#include "../utils/Posix.hpp"
#include <algorithm>
#include <limits>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
namespace metatex::io {
namespace {
int openFIFO(const std::filesystem::path& path) {
    detail::FD fd(::open(path.c_str(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW));
    if (fd.get() < 0) detail::fail("open FIFO writer");
    struct stat info{};
    if (::fstat(fd.get(), &info) < 0) detail::fail("stat FIFO");
    if (!S_ISFIFO(info.st_mode)) throw std::invalid_argument("output is not a FIFO");
    return fd.release();
}
}
FIFOSink::FIFOSink(const std::filesystem::path& path) : DescriptorSink(openFIFO(path), true) {}
void FIFOSink::write(std::string_view bytes) {
    // Block SIGPIPE only in this thread, preserving the application's handlers
    // and any signal that was already pending before this write.
    sigset_t mask, previous, pending; ::sigemptyset(&mask); ::sigaddset(&mask, SIGPIPE);
    int error = ::pthread_sigmask(SIG_BLOCK, &mask, &previous);
    if (error) throw std::system_error(error, std::generic_category(), "block FIFO SIGPIPE");
    util::ScopeExit restore([&] { ::pthread_sigmask(SIG_SETMASK, &previous, nullptr); });
    if (::sigpending(&pending) < 0) detail::fail("pending FIFO signals");
    bool alreadyPending = ::sigismember(&pending, SIGPIPE) == 1;
    try { DescriptorSink::write(bytes); }
    catch (const std::system_error& failure) {
        if (failure.code().value() == EPIPE && !alreadyPending) {
            timespec immediate{};
            while (::sigtimedwait(&mask, nullptr, &immediate) < 0 && errno == EINTR) {}
        }
        throw;
    }
}
SharedMemorySink::SharedMemorySink(std::string name, std::size_t maxBytes) : name_(std::move(name)), limit_(maxBytes) {
    detail::ipcName(name_);
}
void SharedMemorySink::write(std::string_view bytes) {
    if (published_) throw std::logic_error("shared memory already published");
    if (bytes.size() > limit_ - bytes_.size()) throw std::length_error("shared memory output limit exceeded");
    bytes_.append(bytes);
}
void SharedMemorySink::flush() {
    if (published_) return;
    if (bytes_.size() > static_cast<std::uintmax_t>(std::numeric_limits<off_t>::max()))
        throw std::length_error("shared memory size overflow");
    detail::FD fd(::shm_open(name_.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600));
    if (fd.get() < 0) detail::fail("create shared memory");
    util::ScopeExit rollback([&] { ::shm_unlink(name_.c_str()); });
    if (::ftruncate(fd.get(), static_cast<off_t>(bytes_.size())) < 0) detail::fail("size shared memory");
    std::size_t at = 0;
    while (at < bytes_.size()) {
        auto n = ::pwrite(fd.get(), bytes_.data() + at, bytes_.size() - at, static_cast<off_t>(at));
        if (n < 0) { if (errno == EINTR) continue; detail::fail("write shared memory"); }
        if (!n) throw std::runtime_error("zero-length shared memory write");
        at += static_cast<std::size_t>(n);
    }
    published_ = true; rollback.release(); bytes_.clear();
}
MSGQueueSink::MSGQueueSink(const std::string& name, bool create, std::chrono::milliseconds timeout, long messageSize)
    : timeout_(timeout) {
    detail::ipcName(name); (void)detail::deadline(timeout);
    if (messageSize <= 0 || messageSize > 16 * 1024 * 1024) throw std::invalid_argument("invalid message size");
    mq_attr attr{}; attr.mq_maxmsg = 10; attr.mq_msgsize = messageSize;
    queue_ = ::mq_open(name.c_str(), O_WRONLY | O_CLOEXEC | (create ? O_CREAT | O_EXCL : 0), 0600, &attr);
    if (queue_ == static_cast<mqd_t>(-1)) detail::fail("open message queue writer");
    try {
        if (::mq_getattr(queue_, &attr) < 0) detail::fail("message queue attributes");
        messageSize_ = static_cast<std::size_t>(attr.mq_msgsize);
    } catch (...) { ::mq_close(queue_); if (create) ::mq_unlink(name.c_str()); throw; }
}
MSGQueueSink::~MSGQueueSink() { ::mq_close(queue_); }
void MSGQueueSink::send(std::string_view bytes, const timespec& until) {
    int result;
    do { result = ::mq_timedsend(queue_, bytes.empty() ? "" : bytes.data(), bytes.size(), 0, &until); }
    while (result < 0 && errno == EINTR);
    if (result < 0) detail::fail("send message queue");
}
void MSGQueueSink::write(std::string_view bytes) {
    if (finished_) throw std::logic_error("message queue stream already finished");
    auto until = detail::realtimeDeadline(timeout_);
    while (!bytes.empty()) {
        auto n = std::min(bytes.size(), messageSize_);
        send(bytes.substr(0, n), until); bytes.remove_prefix(n);
    }
}
void MSGQueueSink::finish() {
    if (finished_) return;
    send({}, detail::realtimeDeadline(timeout_)); finished_ = true;
}
}
