// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>

#include <fmt/format.h>

#ifdef _WIN32
#include <share.h>   // For _SH_DENYWR
#include <windows.h> // For OutputDebugStringW
#else
#define _SH_DENYWR 0
#endif

#include <boost/algorithm/string/replace.hpp>
#ifdef CITRA_LINUX_GCC_BACKTRACE
#define BOOST_STACKTRACE_USE_BACKTRACE
#include <boost/stacktrace.hpp>
#undef BOOST_STACKTRACE_USE_BACKTRACE
#include <signal.h>
#endif

#include "common/bounded_threadsafe_queue.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/literals.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/logging/log_entry.h"
#include "common/logging/text_formatter.h"
#include "common/polyfill_thread.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "common/thread.h"

namespace Common::Log {

namespace {

/**
 * Interface for logging backends.
 */
class Backend {
public:
    virtual ~Backend() = default;

    virtual void Write(const Entry& entry) = 0;

    virtual void EnableForStacktrace() = 0;

    virtual void Flush() = 0;
};

/**
 * Backend that writes to stderr and with color
 */
class ColorConsoleBackend final : public Backend {
public:
    explicit ColorConsoleBackend() = default;

    ~ColorConsoleBackend() override = default;

    void Write(const Entry& entry) override {
        if (enabled.load(std::memory_order_relaxed)) {
            PrintColoredMessage(entry);
        }
    }

    void Flush() override {
        // stderr shouldn't be buffered
    }

    void EnableForStacktrace() override {
        enabled = true;
    }

    void SetEnabled(bool enabled_) {
        enabled = enabled_;
    }

private:
    std::atomic_bool enabled{false};
};

/**
 * Backend that writes to a file passed into the constructor
 */
class FileBackend final : public Backend {
public:
    explicit FileBackend(const std::string& filename) {
        auto old_filename = filename;
        boost::replace_all(old_filename, ".txt", ".old.txt");

        // Existence checks are done within the functions themselves.
        // We don't particularly care if these succeed or not.
        static_cast<void>(FileUtil::Delete(old_filename));
        static_cast<void>(FileUtil::Rename(filename, old_filename));

        // _SH_DENYWR allows read only access to the file for other programs.
        // It is #defined to 0 on other platforms
        file = std::make_unique<FileUtil::IOFile>(filename, "w", _SH_DENYWR);
    }

    ~FileBackend() override = default;

    void Write(const Entry& entry) override {
        if (!enabled) {
            return;
        }

        bytes_written += file->WriteString(FormatLogMessage(entry).append(1, '\n'));

        using namespace Common::Literals;
        // Prevent logs from exceeding a set maximum size in the event that log entries are spammed.
        const auto write_limit = 100_MiB;
        const bool write_limit_exceeded = bytes_written > write_limit;
        if (entry.log_level >= Level::Error || write_limit_exceeded) {
            if (write_limit_exceeded) {
                // Stop writing after the write limit is exceeded.
                // Don't close the file so we can print a stacktrace if necessary
                enabled = false;
            }
            file->Flush();
        }
    }

    void Flush() override {
        file->Flush();
    }

    void EnableForStacktrace() override {
        enabled = true;
        bytes_written = 0;
    }

private:
    std::unique_ptr<FileUtil::IOFile> file;
    bool enabled = true;
    std::size_t bytes_written = 0;
};

/**
 * Backend that writes to Visual Studio's output window
 */
class DebuggerBackend final : public Backend {
public:
    explicit DebuggerBackend() = default;

    ~DebuggerBackend() override = default;

    void Write(const Entry& entry) override {
#ifdef _WIN32
        ::OutputDebugStringW(UTF8ToUTF16W(FormatLogMessage(entry).append(1, '\n')).c_str());
#endif
    }

    void Flush() override {}

    void EnableForStacktrace() override {}
};

#ifdef ANDROID
/**
 * Backend that writes to the Android logcat
 */
class LogcatBackend : public Backend {
public:
    explicit LogcatBackend() = default;

    ~LogcatBackend() override = default;

    void Write(const Entry& entry) override {
        PrintMessageToLogcat(entry);
    }

    void Flush() override {}

    void EnableForStacktrace() override {}
};
#endif

bool initialization_in_progress_suppress_logging = true;

#ifdef CITRA_LINUX_GCC_BACKTRACE
[[noreturn]] void SleepForever() {
    while (true) {
        pause();
    }
}
#endif

/**
 * Static state as a singleton.
 */
class Impl {
public:
    static Impl& Instance() {
        if (!instance) {
            throw std::runtime_error("Using Logging instance before its initialization");
        }
        return *instance;
    }

    static void Initialize(std::string_view log_file) {
        if (instance) {
            LOG_WARNING(Log, "Reinitializing logging backend");
            return;
        }
        initialization_in_progress_suppress_logging = true;
        const auto& log_dir = FileUtil::GetUserPath(FileUtil::UserPath::LogDir);
        void(FileUtil::CreateDir(log_dir));
        Filter filter;
        filter.ParseFilterString(Settings::values.log_filter.GetValue());
        instance = std::unique_ptr<Impl, decltype(&Deleter)>(
            new Impl(fmt::format("{}{}", log_dir, log_file), filter), Deleter);
        initialization_in_progress_suppress_logging = false;
    }

    static void Start() {
        instance->StartBackendThread();
    }

    static void Stop() {
        instance->StopBackendThread();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void SetGlobalFilter(const Filter& f) {
        filter = f;
    }

    void SetColorConsoleBackendEnabled(bool enabled) {
        color_console_backend.SetEnabled(enabled);
    }

    void PushEntry(Class log_class, Level log_level, const char* filename, unsigned int line_num,
                   const char* function, std::string message) {
        if (!filter.CheckMessage(log_class, log_level)) {
            return;
        }
        message_queue.EmplaceWait(
            CreateEntry(log_class, log_level, filename, line_num, function, std::move(message)));
    }

private:
    Impl(const std::string& file_backend_filename, const Filter& filter_)
        : filter{filter_}, file_backend{file_backend_filename} {
#ifdef CITRA_LINUX_GCC_BACKTRACE
        int waker_pipefd[2];
        int done_printing_pipefd[2];
        if (pipe2(waker_pipefd, O_CLOEXEC) || pipe2(done_printing_pipefd, O_CLOEXEC)) {
            abort();
        }
        backtrace_thread_waker_fd = waker_pipefd[1];
        backtrace_done_printing_fd = done_printing_pipefd[0];
        std::thread([this, wait_fd = waker_pipefd[0], done_fd = done_printing_pipefd[1]] {
            Common::SetCurrentThreadName("citra:Crash");
            for (u8 ignore = 0; read(wait_fd, &ignore, 1) != 1;)
                ;
            const int sig = received_signal;
            if (sig <= 0) {
                abort();
            }
            backend_thread.request_stop();
            backend_thread.join();
            const auto signal_entry =
                CreateEntry(Class::Log, Level::Critical, "?", 0, "?",
                            fmt::vformat("Received signal {}", fmt::make_format_args(sig)));
            ForEachBackend([&signal_entry](Backend& backend) {
                backend.EnableForStacktrace();
                backend.Write(signal_entry);
            });
            const auto backtrace =
                boost::stacktrace::stacktrace::from_dump(backtrace_storage.data(), 4096);
            for (const auto& frame : backtrace.as_vector()) {
                auto line = boost::stacktrace::detail::to_string(&frame, 1);
                if (line.empty()) {
                    abort();
                }
                line.pop_back(); // Remove newline
                const auto frame_entry =
                    CreateEntry(Class::Log, Level::Critical, "?", 0, "?", std::move(line));
                ForEachBackend([&frame_entry](Backend& backend) { backend.Write(frame_entry); });
            }
            using namespace std::literals;
            const auto rip_entry = CreateEntry(Class::Log, Level::Critical, "?", 0, "?", "RIP"s);
            ForEachBackend([&rip_entry](Backend& backend) {
                backend.Write(rip_entry);
                backend.Flush();
            });
            for (const u8 anything = 0; write(done_fd, &anything, 1) != 1;)
                ;
            // Abort on original thread to help debugging
            SleepForever();
        }).detach();
        signal(SIGSEGV, &HandleSignal);
        signal(SIGABRT, &HandleSignal);
#endif
    }

    ~Impl() {
#ifdef CITRA_LINUX_GCC_BACKTRACE
        if (int zero_or_ignore = 0;
            !received_signal.compare_exchange_strong(zero_or_ignore, SIGKILL)) {
            SleepForever();
        }
#endif
    }

    void StartBackendThread() {
        backend_thread = std::jthread([this](std::stop_token stop_token) {
            Common::SetCurrentThreadName("citra:Log");
            Entry entry;
            const auto write_logs = [this, &entry]() {
                ForEachBackend([&entry](Backend& backend) { backend.Write(entry); });
            };
            while (!stop_token.stop_requested()) {
                message_queue.PopWait(entry, stop_token);
                if (entry.filename != nullptr) {
                    write_logs();
                }
            }
            // Drain the logging queue. Only writes out up to MAX_LOGS_TO_WRITE to prevent a
            // case where a system is repeatedly spamming logs even on close.
            int max_logs_to_write = filter.IsDebug() ? INT_MAX : 100;
            while (max_logs_to_write-- && message_queue.TryPop(entry)) {
                write_logs();
            }
        });
    }

    void StopBackendThread() {
        backend_thread.request_stop();
        if (backend_thread.joinable()) {
            backend_thread.join();
        }

        ForEachBackend([](Backend& backend) { backend.Flush(); });
    }

    Entry CreateEntry(Class log_class, Level log_level, const char* filename, unsigned int line_nr,
                      const char* function, std::string&& message) const {
        using std::chrono::duration_cast;
        using std::chrono::microseconds;
        using std::chrono::steady_clock;

        return {
            .timestamp = duration_cast<microseconds>(steady_clock::now() - time_origin),
            .log_class = log_class,
            .log_level = log_level,
            .filename = filename,
            .line_num = line_nr,
            .function = function,
            .message = std::move(message),
        };
    }

    void ForEachBackend(auto lambda) {
        lambda(static_cast<Backend&>(debugger_backend));
        lambda(static_cast<Backend&>(color_console_backend));
        lambda(static_cast<Backend&>(file_backend));
#ifdef ANDROID
        lambda(static_cast<Backend&>(lc_backend));
#endif
    }

    static void Deleter(Impl* ptr) {
        delete ptr;
    }

#ifdef CITRA_LINUX_GCC_BACKTRACE
    [[noreturn]] static void HandleSignal(int sig) {
        signal(SIGABRT, SIG_DFL);
        signal(SIGSEGV, SIG_DFL);
        if (sig <= 0) {
            abort();
        }
        instance->InstanceHandleSignal(sig);
    }

    [[noreturn]] void InstanceHandleSignal(int sig) {
        if (int zero_or_ignore = 0; !received_signal.compare_exchange_strong(zero_or_ignore, sig)) {
            if (received_signal == SIGKILL) {
                abort();
            }
            SleepForever();
        }
        // Don't restart like boost suggests. We want to append to the log file and not lose dynamic
        // symbols. This may segfault if it unwinds outside C/C++ code but we'll just have to fall
        // back to core dumps.
        boost::stacktrace::safe_dump_to(backtrace_storage.data(), 4096);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        for (const int anything = 0; write(backtrace_thread_waker_fd, &anything, 1) != 1;)
            ;
        for (u8 ignore = 0; read(backtrace_done_printing_fd, &ignore, 1) != 1;)
            ;
        abort();
    }
#endif

    static inline std::unique_ptr<Impl, decltype(&Deleter)> instance{nullptr, Deleter};

    Filter filter;
    DebuggerBackend debugger_backend{};
    ColorConsoleBackend color_console_backend{};
    FileBackend file_backend;
#ifdef ANDROID
    LogcatBackend lc_backend{};
#endif

    MPSCQueue<Entry> message_queue{};
    std::chrono::steady_clock::time_point time_origin{std::chrono::steady_clock::now()};
    std::jthread backend_thread;

#ifdef CITRA_LINUX_GCC_BACKTRACE
    std::atomic_int received_signal{0};
    std::array<u8, 4096> backtrace_storage{};
    int backtrace_thread_waker_fd;
    int backtrace_done_printing_fd;
#endif
};
} // namespace

void Initialize(std::string_view log_file) {
    Impl::Initialize(log_file.empty() ? LOG_FILE : log_file);
}

void Start() {
    Impl::Start();
}

void Stop() {
    Impl::Stop();
}

void DisableLoggingInTests() {
    initialization_in_progress_suppress_logging = true;
}

void SetGlobalFilter(const Filter& filter) {
    Impl::Instance().SetGlobalFilter(filter);
}

void SetColorConsoleBackendEnabled(bool enabled) {
    Impl::Instance().SetColorConsoleBackendEnabled(enabled);
}

void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename,
                       unsigned int line_num, const char* function, fmt::string_view format,
                       const fmt::format_args& args) {
    if (!initialization_in_progress_suppress_logging) {
        Impl::Instance().PushEntry(log_class, log_level, filename, line_num, function,
                                   fmt::vformat(format, args));
    }
}
} // namespace Common::Log
