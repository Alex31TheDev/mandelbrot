#include <tuple>

template <typename T, typename Lock>
    requires _ThreadSafeQueueImpl::isLockable<Lock>

void ThreadSafeQueue<T, Lock>::pushBack(T &&value) {
    std::scoped_lock lock(_mutex);
    _data.push_back(std::forward<T>(value));
}

template <typename T, typename Lock>
    requires _ThreadSafeQueueImpl::isLockable<Lock>

void ThreadSafeQueue<T, Lock>::pushFront(T &&value) {
    std::scoped_lock lock(_mutex);
    _data.push_front(std::forward<T>(value));
}

template <typename T, typename Lock>
    requires _ThreadSafeQueueImpl::isLockable<Lock>

std::optional<T> ThreadSafeQueue<T, Lock>::popFront() {
    std::scoped_lock lock(_mutex);
    if (_data.empty()) return std::nullopt;

    std::optional<T> front = std::move(_data.front());
    _data.pop_front();

    return front;
}

template <typename T, typename Lock>
    requires _ThreadSafeQueueImpl::isLockable<Lock>

std::optional<T> ThreadSafeQueue<T, Lock>::popBack() {
    std::scoped_lock lock(_mutex);
    if (_data.empty()) return std::nullopt;

    std::optional<T> back = std::move(_data.back());
    _data.pop_back();

    return back;
}

template <typename T, typename Lock>
    requires _ThreadSafeQueueImpl::isLockable<Lock>

std::optional<T> ThreadSafeQueue<T, Lock>::steal() {
    std::scoped_lock lock(_mutex);
    if (_data.empty()) return std::nullopt;

    std::optional<T> back = std::move(_data.back());
    _data.pop_back();

    return back;
}

template <typename T, typename Lock>
    requires _ThreadSafeQueueImpl::isLockable<Lock>

void ThreadSafeQueue<T, Lock>::rotateToFront(const T &item) {
    std::scoped_lock lock(_mutex);
    auto iter = std::find(_data.begin(), _data.end(), item);

    if (iter != _data.end()) {
        std::ignore = _data.erase(iter);
    }

    _data.push_front(item);
}

template <typename T, typename Lock>
    requires _ThreadSafeQueueImpl::isLockable<Lock>

std::optional<T> ThreadSafeQueue<T, Lock>::copyFrontRotToBack() {
    std::scoped_lock lock(_mutex);
    if (_data.empty()) return std::nullopt;

    std::optional<T> front = _data.front();
    _data.pop_front();

    _data.push_back(*front);
    return front;
}

template <typename T, typename Lock>
    requires _ThreadSafeQueueImpl::isLockable<Lock>

typename ThreadSafeQueue<T, Lock>::SizeType
ThreadSafeQueue<T, Lock>::clear() {
    std::scoped_lock lock(_mutex);

    auto size = _data.size();
    _data.clear();

    return size;
}