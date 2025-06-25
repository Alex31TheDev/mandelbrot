#include <exception>
#include <stop_token>
#include <tuple>

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

template <typename InitFunction>
    requires _ThreadPoolImpl::VoidInvocableWith<InitFunction, size_t>
ThreadPool<FunctionType, ThreadType>::ThreadPool(
    unsigned int threadCount, InitFunction init
) : _tasks(threadCount) {
    size_t current = 0;

    for (size_t i = 0; i < threadCount; i++) {
        _priorityQueue.pushBack(size_t(current));

        try {
            _threads.emplace_back(_threadMain(init, current));
            current++;
        } catch (...) {
            _tasks.pop_back();
            std::ignore = _priorityQueue.popBack();
        }
    }
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

ThreadPool<FunctionType, ThreadType>::~ThreadPool() {
    waitForTasks();

    for (size_t i = 0; i < _threads.size(); i++) {
        _threads[i].request_stop();
        _tasks[i].signal.release();
        _threads[i].join();
    }
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

template <typename Function, typename... Args, typename ReturnType>
    requires _ThreadPoolImpl::InvocableWith<Function, Args...>
std::future<ReturnType>
ThreadPool<FunctionType, ThreadType>::enqueue(Function f, Args... args) {
    auto sharedPromise = std::make_shared<std::promise<ReturnType>>();

    auto task = _makeTaskWithPromise(std::move(f),
        std::move(args)..., sharedPromise);

    auto future = sharedPromise->get_future();
    _enqueueTask(std::move(task));

    return future;
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

template <typename Function, typename... Args>
    requires _ThreadPoolImpl::InvocableWith<Function, Args...>
void ThreadPool<FunctionType, ThreadType>::enqueueDetach(Function &&func, Args &&...args) {
    auto task = _makeDetachedTask(std::forward<Function>(func),
        std::forward<Args>(args)...);

    _enqueueTask(std::move(task));
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

void ThreadPool<FunctionType, ThreadType>::waitForTasks() {
    while (_inFlight.load(std::memory_order_acquire) > 0) {
        _threadsComplete.wait(false);
    }
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

size_t ThreadPool<FunctionType, ThreadType>::clearTasks() {
    size_t removed = 0;

    for (auto &taskList : _tasks) {
        removed += taskList.tasks.clear();
    }

    _inFlight.fetch_sub(removed, std::memory_order_release);
    _unassigned.fetch_sub(removed, std::memory_order_release);

    return removed;
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

template <typename InitFunction>
auto ThreadPool<FunctionType, ThreadType>::_threadMain(InitFunction init, size_t id) {
    return [this, id, init](const std::stop_token &stopToken) {
        try {
            std::invoke(init, id);
        } catch (...) {}

        do {
            _tasks[id].signal.acquire();

            do {
                while (auto task = _tasks[id].tasks.popFront()) {
                    _unassigned.fetch_sub(1, std::memory_order_release);
                    std::invoke(std::move(task.value()));
                    _inFlight.fetch_sub(1, std::memory_order_release);
                }

                for (size_t j = 1; j < _tasks.size(); j++) {
                    const size_t index = (id + j) % _tasks.size();

                    if (auto task = _tasks[index].tasks.steal()) {
                        _unassigned.fetch_sub(1, std::memory_order_release);
                        std::invoke(std::move(task.value()));
                        _inFlight.fetch_sub(1, std::memory_order_release);
                        break;
                    }
                }
            } while (_unassigned.load(std::memory_order_acquire) > 0);

            _priorityQueue.rotateToFront(id);

            if (_inFlight.load(std::memory_order_acquire) == 0) {
                _threadsComplete.store(true, std::memory_order_release);
                _threadsComplete.notify_one();
            }
        } while (!stopToken.stop_requested());
        };
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

template <typename Function, typename... Args, typename ReturnType>
auto ThreadPool<FunctionType, ThreadType>::_makeTaskWithPromise(
    Function &&func, Args &&...args,
    std::shared_ptr<std::promise<ReturnType>> promise
) {
    return [f = std::forward<Function>(func),
        ... largs = std::forward<Args>(args), promise]() mutable {
        try {
            if constexpr (std::is_same_v<ReturnType, void>) {
                std::invoke(f, largs...);
                promise->set_value();
            } else {
                promise->set_value(std::invoke(f, largs...));
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
        };
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

template <typename Function, typename... Args>
auto ThreadPool<FunctionType, ThreadType>::_makeDetachedTask(
    Function &&func, Args &&...args
) {
    return [f = std::forward<Function>(func),
        ... largs = std::forward<Args>(args)]() mutable {
        try {
            if constexpr (std::is_same_v<void,
                std::invoke_result_t<Function &&, Args &&...>>) {
                std::invoke(f, largs...);
            } else {
                std::ignore = std::invoke(f, largs...);
            }
        } catch (...) {}
        };
}

template <typename FunctionType, typename ThreadType>
    requires _ThreadPoolImpl::VoidInvocable<FunctionType>

template <typename Function>
void ThreadPool<FunctionType, ThreadType>::_enqueueTask(Function &&f) {
    auto i_opt = _priorityQueue.copyFrontRotToBack();
    if (!i_opt.has_value()) return;

    _unassigned.fetch_add(1, std::memory_order_release);
    const auto prevInFlight = _inFlight.fetch_add(1,
        std::memory_order_release);

    if (prevInFlight == 0) {
        _threadsComplete.store(false, std::memory_order_release);
    }

    auto i = *(i_opt);
    _tasks[i].tasks.pushBack(std::forward<Function>(f));
    _tasks[i].signal.release();
}
