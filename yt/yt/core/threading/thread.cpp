#include "thread.h"

#include "private.h"

#include <yt/yt/core/actions/bind.h>

#include <yt/yt/core/misc/proc.h>

#include <library/cpp/yt/misc/tls.h>

#include <util/generic/size_literals.h>

#ifdef _linux_
    #include <sched.h>
#endif

#include <signal.h>

namespace NYT::NThreading {

////////////////////////////////////////////////////////////////////////////////

YT_THREAD_LOCAL(TThreadId) CurrentUniqueThreadId;
static std::atomic<TThreadId> UniqueThreadIdGenerator;

static const auto& Logger = ThreadingLogger;

////////////////////////////////////////////////////////////////////////////////

TThread::TThread(
    TString threadName,
    EThreadPriority threadPriority,
    int shutdownPriority)
    : ThreadName_(std::move(threadName))
    , ThreadPriority_(threadPriority)
    , ShutdownPriority_(shutdownPriority)
    , UniqueThreadId_(++UniqueThreadIdGenerator)
    , UnderlyingThread_(&StaticThreadMainTrampoline, this)
{ }

TThread::~TThread()
{
    Stop();
}

TThreadId TThread::GetThreadId() const
{
    return ThreadId_;
}

TString TThread::GetThreadName() const
{
    return ThreadName_;
}

bool TThread::StartSlow()
{
    auto guard = Guard(SpinLock_);

    if (Started_.load()) {
        return !Stopping_.load();
    }

    if (Stopping_.load()) {
        // Stopped without being started.
        return false;
    }

    ShutdownCookie_ = RegisterShutdownCallback(
        Format("Thread(%v)", ThreadName_),
        BIND_NO_PROPAGATE(&TThread::Stop, MakeWeak(this)),
        ShutdownPriority_);
    if (!ShutdownCookie_) {
        Stopping_ = true;
        return false;
    }

    if (auto* logFile = TryGetShutdownLogFile()) {
        ::fprintf(logFile, "%s\t*** Starting thread (ThreadName: %s)\n",
            GetInstant().ToString().c_str(),
            ThreadName_.c_str());
    }

    StartPrologue();

    try {
        UnderlyingThread_.Start();
    } catch (const std::exception& ex) {
        fprintf(stderr, "%s\t*** Error starting thread (ThreadName: %s)\n*** %s\n",
            GetInstant().ToString().c_str(),
            ThreadName_.c_str(),
            ex.what());
        YT_ABORT();
    }

    Started_ = true;

    StartedEvent_.Wait();

    StartEpilogue();

    if (auto* logFile = TryGetShutdownLogFile()) {
        ::fprintf(logFile, "%s\t*** Thread started (ThreadName: %s, ThreadId: %" PRISZT ")\n",
            GetInstant().ToString().c_str(),
            ThreadName_.c_str(),
            ThreadId_);
    }

    return true;
}

bool TThread::CanWaitForThreadShutdown() const
{
    return
        CurrentUniqueThreadId != UniqueThreadId_ &&
        GetShutdownThreadId() != ThreadId_;
}

void TThread::Stop()
{
    {
        auto guard = Guard(SpinLock_);
        auto alreadyStopping = Stopping_.exchange(true);
        if (!Started_) {
            return;
        }
        if (alreadyStopping) {
            guard.Release();
            // Avoid deadlock.
            if (CanWaitForThreadShutdown()) {
                if (auto* logFile = TryGetShutdownLogFile()) {
                    ::fprintf(logFile, "%s\t*** Waiting for an already stopping thread to finish (ThreadName: %s, ThreadId: %" PRISZT ", WaiterThreadId: %" PRISZT ")\n",
                        GetInstant().ToString().c_str(),
                        ThreadName_.c_str(),
                        ThreadId_,
                        GetCurrentThreadId());
                }
                StoppedEvent_.Wait();
            } else {
                if (auto* logFile = TryGetShutdownLogFile()) {
                    ::fprintf(logFile, "%s\t*** Cannot wait for an already stopping thread to finish (ThreadName: %s, ThreadId: %" PRISZT ", WaiterThreadId: %" PRISZT ")\n",
                        GetInstant().ToString().c_str(),
                        ThreadName_.c_str(),
                        ThreadId_,
                        GetCurrentThreadId());
                }
            }
            return;
        }
    }

    if (auto* logFile = TryGetShutdownLogFile()) {
        ::fprintf(logFile, "%s\t*** Stopping thread (ThreadName: %s, ThreadId: %" PRISZT ", RequesterThreadId: %" PRISZT ")\n",
            GetInstant().ToString().c_str(),
            ThreadName_.c_str(),
            ThreadId_,
            GetCurrentThreadId());
    }

    StopPrologue();

    // Avoid deadlock.
    if (CanWaitForThreadShutdown()) {
        if (auto* logFile = TryGetShutdownLogFile()) {
            ::fprintf(logFile, "%s\t*** Waiting for thread to stop (ThreadName: %s, ThreadId: %" PRISZT ", RequesterThreadId: %" PRISZT ")\n",
                GetInstant().ToString().c_str(),
                ThreadName_.c_str(),
                ThreadId_,
                GetCurrentThreadId());
        }
        UnderlyingThread_.Join();
    } else {
        if (auto* logFile = TryGetShutdownLogFile()) {
            ::fprintf(logFile, "%s\t*** Cannot wait for thread to stop; detaching (ThreadName: %s, ThreadId: %" PRISZT ", RequesterThreadId: %" PRISZT ")\n",
                GetInstant().ToString().c_str(),
                ThreadName_.c_str(),
                ThreadId_,
                GetCurrentThreadId());
        }
        UnderlyingThread_.Detach();
    }

    StopEpilogue();

    if (auto* logFile = TryGetShutdownLogFile()) {
        ::fprintf(logFile, "%s\t*** Thread stopped (ThreadName: %s, ThreadId: %" PRISZT ", RequesterThreadId: %" PRISZT ")\n",
            GetInstant().ToString().c_str(),
            ThreadName_.c_str(),
            ThreadId_,
            GetCurrentThreadId());
    }
}

void* TThread::StaticThreadMainTrampoline(void* opaque)
{
    reinterpret_cast<TThread*>(opaque)->ThreadMainTrampoline();
    return nullptr;
}

void TThread::ThreadMainTrampoline()
{
    auto this_ = MakeStrong(this);

    ::TThread::SetCurrentThreadName(ThreadName_.c_str());

    ThreadId_ = GetCurrentThreadId();
    CurrentUniqueThreadId = UniqueThreadId_;

    SetThreadPriority();
    ConfigureSignalHandlerStack();

    StartedEvent_.NotifyAll();

    class TExitInterceptor
    {
    public:
        ~TExitInterceptor()
        {
            if (Armed_ && !std::uncaught_exceptions()) {
                if (auto* logFile = TryGetShutdownLogFile()) {
                    ::fprintf(logFile, "%s\tThread exit interceptor triggered (ThreadId: %" PRISZT ")\n",
                        GetInstant().ToString().c_str(),
                        GetCurrentThreadId());
                }
                Shutdown();
            }
        }

        void Disarm()
        {
            Armed_ = false;
        }

    private:
        bool Armed_ = true;
    };

    YT_THREAD_LOCAL(TExitInterceptor) Interceptor;

    ThreadMain();

    GetTlsRef(Interceptor).Disarm();

    StoppedEvent_.NotifyAll();
}

void TThread::StartPrologue()
{ }

void TThread::StartEpilogue()
{ }

void TThread::StopPrologue()
{ }

void TThread::StopEpilogue()
{ }

void TThread::SetThreadPriority()
{
    YT_VERIFY(ThreadId_ != InvalidThreadId);

#ifdef _linux_
    if (ThreadPriority_ == EThreadPriority::RealTime) {
        struct sched_param param{
            .sched_priority = 1
        };
        int result = sched_setscheduler(ThreadId_, SCHED_FIFO, &param);
        if (result == 0) {
            YT_LOG_DEBUG("Thread real-time priority enabled (ThreadName: %v)",
                ThreadName_);
        } else {
            YT_LOG_DEBUG(TError::FromSystem(), "Cannot enable thread real-time priority: sched_setscheduler failed (ThreadName: %v)",
                ThreadName_);
        }
    }
#else
    Y_UNUSED(ThreadPriority_);
    Y_UNUSED(Logger);
#endif
}

void TThread::ConfigureSignalHandlerStack()
{
#if !defined(_asan_enabled_) && !defined(_msan_enabled_) && \
    (_XOPEN_SOURCE >= 500 || \
    /* Since glibc 2.12: */ _POSIX_C_SOURCE >= 200809L || \
    /* glibc <= 2.19: */ _BSD_SOURCE)
    YT_THREAD_LOCAL(bool) Configured;
    if (std::exchange(Configured, true)) {
        return;
    }

    // The size of of the custom stack to be provided for signal handlers.
    constexpr size_t SignalHandlerStackSize = 16_KB;
    YT_THREAD_LOCAL(std::array<char, SignalHandlerStackSize>) Stack;

    stack_t stack{
        .ss_sp = GetTlsRef(Stack).data(),
        .ss_flags = 0,
        .ss_size = GetTlsRef(Stack).size(),
    };
    YT_VERIFY(sigaltstack(&stack, nullptr) == 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NThreading
