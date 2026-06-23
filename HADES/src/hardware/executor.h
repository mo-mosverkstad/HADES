#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// Internal execution helper. Not part of public API.
// Drives a step function on a background thread with start/stop/wait semantics.
class Executor {
public:
    using StepFn = std::function<void()>;
    using HaltedFn = std::function<bool()>;

    Executor(StepFn step, HaltedFn halted)
        : step_(std::move(step)), halted_(std::move(halted)) {}

    ~Executor() {
        if (thread_.joinable()) {
            stop_requested_ = true;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                shutdown_ = true;
                run_signaled_ = true;
                cv_run_.notify_one();
            }
            thread_.join();
        }
    }

    void run_async(uint64_t budget) {
        if (running_) return;
        if (!thread_.joinable()) {
            thread_ = std::thread(&Executor::thread_main, this);
        }
        stop_requested_ = false;
        std::lock_guard<std::mutex> lk(mutex_);
        budget_ = budget;
        run_signaled_ = true;
        cv_run_.notify_one();
    }

    void wait() {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_done_.wait(lk, [this]{ return done_signaled_; });
        done_signaled_ = false;
    }

    void stop() { stop_requested_ = true; }
    bool is_running() const { return running_.load(std::memory_order_relaxed); }

private:
    StepFn step_;
    HaltedFn halted_;

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_run_;
    std::condition_variable cv_done_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    bool shutdown_ = false;
    uint64_t budget_ = 0;
    bool run_signaled_ = false;
    bool done_signaled_ = false;

    void thread_main() {
        while (true) {
            {
                std::unique_lock<std::mutex> lk(mutex_);
                cv_run_.wait(lk, [this]{ return run_signaled_; });
                run_signaled_ = false;
            }
            if (shutdown_) return;
            running_ = true;

            uint64_t count = 0;
            uint64_t limit = (budget_ == 0) ? UINT64_MAX : budget_;
            while (count < limit) {
                if (stop_requested_.load(std::memory_order_relaxed)) break;
                if (halted_()) break;
                step_();
                count++;
            }

            running_ = false;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                done_signaled_ = true;
                cv_done_.notify_one();
            }
        }
    }
};
