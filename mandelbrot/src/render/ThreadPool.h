#pragma once

#include <atomic>
#include <concepts>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <semaphore>
#include <thread>
#include <type_traits>
#include <vector>

#include "ThreadSafeQueue.h"

namespace _ThreadPoolImpl {
    using DefaultFuncType = std::function<void()>;

    template <typename F, typename... Args>
    concept InvocableWith = std::invocable<F, Args...>;

    template <typename F>
    concept VoidInvocable = std::invocable<F> &&
        std::is_same_v<void, std::invoke_result_t<F>>;

    template <typename F, typename... Args>
    concept VoidInvocableWith = InvocableWith<F, Args...> &&
        std::is_same_v<void, std::invoke_result_t<F, Args...>>;
}

template <typename FunctionType = _ThreadPoolImpl::DefaultFuncType,
    typename ThreadType = std::jthread>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>
class ThreadPool {
public:
    template <typename InitFunction = std::function<void(size_t)>>
        requires _ThreadPoolImpl::VoidInvocableWith<InitFunction, size_t>
    explicit ThreadPool(
        unsigned int threadCount = std::thread::hardware_concurrency(),
        InitFunction init = [](size_t) {}
    );

    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool(ThreadPool &&) = default;
    ThreadPool &operator=(ThreadPool &&) = default;

    [[nodiscard]] size_t size() const { return _threads.size(); };

    template <typename Function, typename... Args,
        typename ReturnType = std::invoke_result_t<Function &&, Args &&...>>
        requires _ThreadPoolImpl::InvocableWith<Function, Args...>
    [[nodiscard]] std::future<ReturnType> enqueue(Function f, Args... args);

    template <typename Function, typename... Args>
        requires _ThreadPoolImpl::InvocableWith<Function, Args...>
    void enqueueDetach(Function &&func, Args &&...args);

    size_t clearTasks();
    void waitForTasks();

private:
    template <typename InitFunction>
    auto _threadMain(InitFunction init, size_t id);

    template <typename Function, typename... Args, typename ReturnType>
    auto _makeTaskWithPromise(Function &&func, Args &&...args,
        std::shared_ptr<std::promise<ReturnType>> promise);
    template <typename Function, typename... Args>
    auto _makeDetachedTask(Function &&func, Args &&...args);

    template <typename Function>
    void _enqueueTask(Function &&f);

    std::vector<ThreadType> _threads;

    struct _TaskItem {
        ThreadSafeQueue<FunctionType> tasks;
        std::binary_semaphore signal{ 0 };
    };
    std::deque<_TaskItem> _tasks;

    ThreadSafeQueue<size_t> _priorityQueue;

    std::atomic_int_fast64_t _unassigned{ 0 }, _inFlight{ 0 };
    std::atomic_bool _threadsComplete{ false };
};

#include "ThreadPool_impl.h"
