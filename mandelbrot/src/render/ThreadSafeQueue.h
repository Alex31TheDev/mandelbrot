#pragma once

#include <concepts>
#include <deque>
#include <tuple>
#include <mutex>
#include <optional>

template <typename Lock>
concept isLockable = requires(Lock && lock) {
    lock.lock();
    lock.unlock();

    { lock.try_lock() } -> std::convertible_to<bool>;
};

template <typename T, typename Lock = std::mutex>
    requires isLockable<Lock>
class ThreadSafeQueue {
public:
    using ValueType = T;
    using SizeType = typename std::deque<T>::size_type;

    ThreadSafeQueue() = default;

    void pushBack(T &&value) {
        std::scoped_lock lock(_mutex);
        _data.push_back(std::forward<T>(value));
    }

    void pushFront(T &&value) {
        std::scoped_lock lock(_mutex);
        _data.push_front(std::forward<T>(value));
    }

    [[nodiscard]] bool empty() const {
        std::scoped_lock lock(_mutex);
        return _data.empty();
    }

    SizeType clear() {
        std::scoped_lock lock(_mutex);

        auto size = _data.size();
        _data.clear();

        return size;
    }

    [[nodiscard]] std::optional<T> popFront() {
        std::scoped_lock lock(_mutex);
        if (_data.empty()) return std::nullopt;

        std::optional<T> front = std::move(_data.front());
        _data.pop_front();

        return front;
    }

    [[nodiscard]] std::optional<T> popBack() {
        std::scoped_lock lock(_mutex);
        if (_data.empty()) return std::nullopt;

        std::optional<T> back = std::move(_data.back());
        _data.pop_back();

        return back;
    }

    [[nodiscard]] std::optional<T> steal() {
        std::scoped_lock lock(_mutex);
        if (_data.empty()) return std::nullopt;

        std::optional<T> back = std::move(_data.back());
        _data.pop_back();

        return back;
    }

    void rotateToFront(const T &item) {
        std::scoped_lock lock(_mutex);
        auto iter = std::find(_data.begin(), _data.end(), item);

        if (iter != _data.end()) {
            std::ignore = _data.erase(iter);
        }

        _data.push_front(item);
    }

    [[nodiscard]] std::optional<T> copyFrontAndRotateToBack() {
        std::scoped_lock lock(_mutex);
        if (_data.empty()) return std::nullopt;

        std::optional<T> front = _data.front();
        _data.pop_front();

        _data.push_back(*front);
        return front;
    }

private:
    std::deque<T> _data{};
    mutable Lock _mutex{};
};
