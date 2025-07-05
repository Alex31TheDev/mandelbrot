#include <tuple>
#include <algorithm>

#define _CLASS_TEMPLATE \
template <typename T, typename Lock> \
    requires _ThreadSafeQueueImpl::isLockable<Lock>

#define _CLASS_NAME ThreadSafeQueue<T, Lock>

_CLASS_TEMPLATE
void _CLASS_NAME::pushBack(T &&value) {
    std::scoped_lock lock(_mutex);
    _data.push_back(std::forward<T>(value));
}

_CLASS_TEMPLATE
void _CLASS_NAME::pushFront(T &&value) {
    std::scoped_lock lock(_mutex);
    _data.push_front(std::forward<T>(value));
}

_CLASS_TEMPLATE
std::optional<T> _CLASS_NAME::popFront() {
    std::scoped_lock lock(_mutex);
    if (_data.empty()) return std::nullopt;

    std::optional<T> front = std::move(_data.front());
    _data.pop_front();

    return front;
}

_CLASS_TEMPLATE
std::optional<T> _CLASS_NAME::popBack() {
    std::scoped_lock lock(_mutex);
    if (_data.empty()) return std::nullopt;

    std::optional<T> back = std::move(_data.back());
    _data.pop_back();

    return back;
}

_CLASS_TEMPLATE
std::optional<T> _CLASS_NAME::steal() {
    std::scoped_lock lock(_mutex);
    if (_data.empty()) return std::nullopt;

    std::optional<T> back = std::move(_data.back());
    _data.pop_back();

    return back;
}

_CLASS_TEMPLATE
void _CLASS_NAME::rotateToFront(const T &item) {
    std::scoped_lock lock(_mutex);
    auto iter = std::ranges::find(_data, item);

    if (iter != _data.end()) {
        std::ignore = _data.erase(iter);
    }

    _data.push_front(item);
}

_CLASS_TEMPLATE
std::optional<T> _CLASS_NAME::copyFrontRotToBack() {
    std::scoped_lock lock(_mutex);
    if (_data.empty()) return std::nullopt;

    std::optional<T> front = _data.front();
    _data.pop_front();

    _data.push_back(*front);
    return front;
}

_CLASS_TEMPLATE
typename _CLASS_NAME::size_type
_CLASS_NAME::clear() {
    std::scoped_lock lock(_mutex);

    auto size = _data.size();
    _data.clear();

    return size;
}

#undef _CLASS_TEMPLATE
#undef _CLASS_NAME
