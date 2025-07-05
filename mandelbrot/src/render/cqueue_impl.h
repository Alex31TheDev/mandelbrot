#include <algorithm>
#include <stdexcept>

#define _CLASS_TEMPLATE template<std::movable T, typename Allocator>
#define _CLASS_NAME cqueue<T, Allocator>

_CLASS_TEMPLATE
constexpr _CLASS_NAME::cqueue(
    size_type capacity,
    const_alloc_ref_t alloc
) :
    _alloc(alloc) {
    if (capacity > MAX_CAPACITY) {
        throw std::length_error("cqueue max capacity exceeded");
    }

    _capacity = (capacity == 0 ? MAX_CAPACITY : capacity);
}

_CLASS_TEMPLATE
constexpr _CLASS_NAME::cqueue(
    const cqueue &other,
    const_alloc_ref_t alloc
) :
    _alloc{ alloc }, _capacity{ other._capacity } {
    resizeIfRequired(other._length);

    for (size_type i = 0; i < other.size(); ++i) {
        push_back(other[i]);
    }
}

_CLASS_TEMPLATE
constexpr _CLASS_NAME::cqueue(
    cqueue &&other,
    const_alloc_ref_t alloc
) {
    if (alloc == other._alloc) {
        swap(other);
    } else {
        cqueue aux{ other, alloc };
        swap(aux);
    }
}

_CLASS_TEMPLATE
constexpr auto _CLASS_NAME::
operator=(const cqueue &other) -> cqueue & {
    cqueue tmp(other);
    this->swap(tmp);
    return *this;
}

_CLASS_TEMPLATE
constexpr auto _CLASS_NAME::
getUncheckedIndex(size_type pos) const noexcept {
    if (_reserved > 1 && (_reserved & (_reserved - 1)) == 0) {
        return ((_front + pos) & (_reserved - 1));
    } else {
        return ((_front + pos) % (_reserved == 0 ? 1 : _reserved));
    }
}

_CLASS_TEMPLATE
constexpr auto _CLASS_NAME::
getCheckedIndex(size_type pos) const noexcept(false) {
    if (pos >= _length) {
        throw std::out_of_range("cqueue access out of range");
    }

    return getUncheckedIndex(pos);
}

_CLASS_TEMPLATE
constexpr auto _CLASS_NAME::
getNewMemoryLength(size_type n) const noexcept {
    size_type ret = _reserved == 0 ?
        std::min(_capacity, MIN_ALLOCATE) : _reserved;

    while (ret < n) ret *= GROWTH_FACTOR;
    return std::min(ret, _capacity);
}

_CLASS_TEMPLATE
void _CLASS_NAME::resize(size_type len) {
    pointer tmp = alloc_traits::allocate(_alloc, len);

    if constexpr (std::is_nothrow_move_constructible<T>::value) {
        for (size_type i = 0; i < _length; ++i) {
            const size_type index = getUncheckedIndex(i);
            alloc_traits::construct(_alloc, tmp + i, std::move(_data[index]));
        }
    } else {
        size_type pos = 0;

        try {
            for (pos = 0; pos < _length; ++pos) {
                const size_type index = getUncheckedIndex(pos);
                alloc_traits::construct(_alloc, tmp + pos, _data[index]);
            }
        } catch (...) {
            while (pos-- > 0) {
                alloc_traits::destroy(_alloc, tmp + pos);
            }

            alloc_traits::deallocate(_alloc, tmp, len);
            throw;
        }
    }

    for (size_type j = 0; j < _length; ++j) {
        const size_type index = getUncheckedIndex(j);
        alloc_traits::destroy(_alloc, _data + index);
    }

    alloc_traits::deallocate(_alloc, _data, _reserved);

    _data = tmp;
    _reserved = len;
    _front = 0;
}

_CLASS_TEMPLATE
constexpr void _CLASS_NAME::resizeIfRequired(size_type n) {
    if (n <= _reserved) {
        return;
    } else if (n > _capacity) {
        throw std::length_error("cqueue capacity exceeded");
    }

    const size_type len = getNewMemoryLength(n);
    resize(len);
}

_CLASS_TEMPLATE
constexpr void _CLASS_NAME::reserve(size_type n) {
    if (n <= _reserved) {
        return;
    } else if (n > _capacity) {
        throw std::length_error("cqueue capacity exceeded");
    }

    resize(n);
}

_CLASS_TEMPLATE
constexpr void _CLASS_NAME::shrink_to_fit() {
    if (_reserved == 0) {
        return;
    } else if (_length == 0) {
        reset();
    } else if (_length == _reserved || _reserved <= MIN_ALLOCATE) {
        return;
    }

    resize(_length);
}

_CLASS_TEMPLATE
constexpr void _CLASS_NAME::
swap(cqueue &other) noexcept {
    if (&other != this) {
        if constexpr (alloc_traits::propagate_on_container_swap::value) {
            std::swap(_alloc, other._alloc);
        } else if (!alloc_traits::is_always_equal::value &&
            _alloc != other._alloc) {
            //
        }

        std::swap(_data, other._data);
        std::swap(_front, other._front);
        std::swap(_length, other._length);
        std::swap(_reserved, other._reserved);
        std::swap(_capacity, other._capacity);
    }
}

_CLASS_TEMPLATE
void _CLASS_NAME::clear() noexcept {
    for (size_type i = 0; i < _length; ++i) {
        const size_type index = getUncheckedIndex(i);
        alloc_traits::destroy(_alloc, _data + index);
    }

    _front = 0;
    _length = 0;
}

_CLASS_TEMPLATE
void _CLASS_NAME::reset() noexcept {
    clear();
    alloc_traits::deallocate(_alloc, _data, _reserved);

    _data = nullptr;
    _reserved = 0;
}

_CLASS_TEMPLATE
constexpr void _CLASS_NAME::push_back(const T &val) {
    resizeIfRequired(_length + 1);

    const size_type index = getUncheckedIndex(_length);
    alloc_traits::construct(_alloc, _data + index, val);

    ++_length;
}

_CLASS_TEMPLATE
constexpr void _CLASS_NAME::push_back(T &&val) {
    resizeIfRequired(_length + 1);

    const size_type index = getUncheckedIndex(_length);
    alloc_traits::construct(_alloc, _data + index, std::move(val));

    ++_length;
}

_CLASS_TEMPLATE
constexpr void _CLASS_NAME::push_front(const T &val) {
    resizeIfRequired(_length + 1);

    const size_type index = (_length == 0 ? 0 :
        (_front == 0 ? _reserved : _front) - 1);
    alloc_traits::construct(_alloc, _data + index, val);

    _front = index;
    ++_length;
}

_CLASS_TEMPLATE
constexpr void _CLASS_NAME::push_front(T &&val) {
    resizeIfRequired(_length + 1);

    const size_type index = (_length == 0 ? 0 :
        (_front == 0 ? _reserved : _front) - 1);
    alloc_traits::construct(_alloc, _data + index, std::move(val));

    _front = index;
    ++_length;
}

_CLASS_TEMPLATE
template <class... Args>
constexpr auto _CLASS_NAME::emplace_back(Args&&... args) -> reference {
    resizeIfRequired(_length + 1);

    const size_type index = getUncheckedIndex(_length);
    alloc_traits::construct(_alloc, _data + index,
        std::forward<Args>(args)...);

    ++_length;
    return _data[index];
}

_CLASS_TEMPLATE
template <class... Args>
constexpr auto _CLASS_NAME::emplace_front(Args&&... args) -> reference {
    resizeIfRequired(_length + 1);

    const size_type index = (_length == 0 ?
        0 : (_front == 0 ? _reserved : _front) - 1);
    alloc_traits::construct(_alloc, _data + index,
        std::forward<Args>(args)...);
    _front = index;

    ++_length;
    return _data[index];
}

_CLASS_TEMPLATE
constexpr typename _CLASS_NAME::value_type
_CLASS_NAME::pop_front() {
    value_type ret{ std::move(front()) };

    alloc_traits::destroy(_alloc, _data + _front);
    _front = getUncheckedIndex(1);

    --_length;
    return ret;
}

_CLASS_TEMPLATE
constexpr typename _CLASS_NAME::value_type
_CLASS_NAME::pop_back() {
    value_type ret{ std::move(back()) };

    const size_type index = getUncheckedIndex(_length - 1);
    alloc_traits::destroy(_alloc, _data + index);

    --_length;
    return ret;
}

#undef _CLASS_TEMPLATE
#undef _CLASS_NAME
