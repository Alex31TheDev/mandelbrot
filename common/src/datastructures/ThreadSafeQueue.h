#pragma once

#include <concepts>
#include <deque>
#include <mutex>
#include <optional>

namespace _ThreadSafeQueueImpl {
    template <typename Lock>
    concept isLockable = requires(Lock && lock) {
        lock.lock();
        lock.unlock();

        { lock.try_lock() } -> std::convertible_to<bool>;
    };
}

template <typename T, typename Lock = std::mutex>
    requires _ThreadSafeQueueImpl::isLockable<Lock>
class ThreadSafeQueue {
public:
    using value_type = T;
    using size_type = typename std::deque<T>::size_type;

    ThreadSafeQueue() = default;

    [[nodiscard]] bool empty() const {
        std::scoped_lock lock(_mutex);
        return _data.empty();
    }

    void pushBack(T &&value);
    void pushFront(T &&value);

    [[nodiscard]] std::optional<T> popFront();
    [[nodiscard]] std::optional<T> popBack();
    [[nodiscard]] std::optional<T> steal();

    void rotateToFront(const T &item);
    [[nodiscard]] std::optional<T> copyFrontRotToBack();

    size_type clear();

private:
    std::deque<T> _data;
    mutable Lock _mutex;
};

#include "ThreadSafeQueue_impl.h"
