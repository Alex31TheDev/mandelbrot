#pragma once

#include <atomic>
#include <concepts>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <semaphore>
#include <stop_token>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#include "ThreadSafeQueue.h"

namespace _impl {
    using DefaultFunctionType = std::function<void()>;
}

template <typename FunctionType = _impl::DefaultFunctionType,
    typename ThreadType = std::jthread>
    requires std::invocable<FunctionType> &&
std::is_same_v<void, std::invoke_result_t<FunctionType>>
class ThreadPool {
public:
    template <typename InitFunction = std::function<void(size_t)>>
        requires std::invocable<InitFunction, size_t> &&
    std::is_same_v<void, std::invoke_result_t<InitFunction, size_t>>
        explicit ThreadPool(
            unsigned int threadCount = std::thread::hardware_concurrency(),
            InitFunction init = [](size_t) {}
        ) : _tasks(threadCount) {
        size_t currentId = 0;

        for (size_t i = 0; i < threadCount; ++i) {
            _priorityQueue.pushBack(size_t(currentId));

            try {
                _threads.emplace_back(
                    [&, id = currentId, init](const std::stop_token &stopToken) {
                        try {
                            std::invoke(init, id);
                        } catch (...) {}

                        do {
                            _tasks[id].signal.acquire();

                            do {
                                while (auto task = _tasks[id].tasks.popFront()) {
                                    _unassingnedTasks.fetch_sub(1, std::memory_order_release);
                                    std::invoke(std::move(task.value()));
                                    _inFlightTasks.fetch_sub(1, std::memory_order_release);
                                }

                                for (size_t j = 1; j < _tasks.size(); ++j) {
                                    const size_t index = (id + j) % _tasks.size();

                                    if (auto task = _tasks[index].tasks.steal()) {
                                        _unassingnedTasks.fetch_sub(1, std::memory_order_release);
                                        std::invoke(std::move(task.value()));
                                        _inFlightTasks.fetch_sub(1, std::memory_order_release);

                                        break;
                                    }
                                }
                            } while (_unassingnedTasks.load(std::memory_order_acquire) > 0);

                            _priorityQueue.rotateToFront(id);

                            if (_inFlightTasks.load(std::memory_order_acquire) == 0) {
                                _threadsCompleteSignal.store(true, std::memory_order_release);
                                _threadsCompleteSignal.notify_one();
                            }
                        } while (!stopToken.stop_requested());
                    }
                );

                ++currentId;
            } catch (...) {
                _tasks.pop_back();
                std::ignore = _priorityQueue.popBack();
            }
        }
    }

    ~ThreadPool() {
        waitForTasks();

        for (size_t i = 0; i < _threads.size(); ++i) {
            _threads[i].request_stop();
            _tasks[i].signal.release();
            _threads[i].join();
        }
    }

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    template <typename Function, typename... Args,
        typename ReturnType = std::invoke_result_t<Function &&, Args &&...>>
        requires std::invocable<Function, Args...>
    [[nodiscard]] std::future<ReturnType> enqueue(Function f, Args... args) {
        auto sharedPromise = std::make_shared<std::promise<ReturnType>>();

        auto task = [func = std::move(f), ... largs = std::move(args),
            promise = sharedPromise]() {
            try {
                if constexpr (std::is_same_v<ReturnType, void>) {
                    func(largs...);
                    promise->set_value();
                } else {
                    promise->set_value(func(largs...));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
            };

        auto future = sharedPromise->get_future();
        _enqueueTask(std::move(task));

        return future;
    }

    template <typename Function, typename... Args>
        requires std::invocable<Function, Args...>
    void enqueueDetach(Function &&func, Args &&...args) {
        _enqueueTask(std::move([f = std::forward<Function>(func), ... largs =
            std::forward<Args>(args)]() mutable -> decltype(auto) {
                try {
                    if constexpr (std::is_same_v<void,
                        std::invoke_result_t<Function &&, Args &&...>>) {
                        std::invoke(f, largs...);
                    } else {
                        std::ignore = std::invoke(f, largs...);
                    }
                } catch (...) {}
            }));
    }

    [[nodiscard]] auto size() const { return _threads.size(); }

    void waitForTasks() {
        while (_inFlightTasks.load(std::memory_order_acquire) > 0) {
            _threadsCompleteSignal.wait(false);
        }
    }

    size_t clearTasks() {
        size_t removedTaskCount = 0;

        for (auto &taskList : _tasks) {
            removedTaskCount += taskList.tasks.clear();
        }

        _inFlightTasks.fetch_sub(removedTaskCount, std::memory_order_release);
        _unassingnedTasks.fetch_sub(removedTaskCount, std::memory_order_release);

        return removedTaskCount;
    }

private:
    template <typename Function>
    void _enqueueTask(Function &&f) {
        auto i_opt = _priorityQueue.copyFrontAndRotateToBack();
        if (!i_opt.has_value()) return;

        _unassingnedTasks.fetch_add(1, std::memory_order_release);
        const auto prev_in_flight = _inFlightTasks.fetch_add(1, std::memory_order_release);

        if (prev_in_flight == 0) {
            _threadsCompleteSignal.store(false, std::memory_order_release);
        }

        auto i = *(i_opt);
        _tasks[i].tasks.pushBack(std::forward<Function>(f));
        _tasks[i].signal.release();
    }

    struct _TaskItem {
        ThreadSafeQueue<FunctionType> tasks{};
        std::binary_semaphore signal{ 0 };
    };

    std::vector<ThreadType> _threads;
    std::deque<_TaskItem> _tasks;
    ThreadSafeQueue<size_t> _priorityQueue;
    std::atomic_int_fast64_t _unassingnedTasks{ 0 }, _inFlightTasks{ 0 };
    std::atomic_bool _threadsCompleteSignal{ false };
};
