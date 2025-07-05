#pragma once

#include <memory>
#include <iterator>
#include <limits>
#include <utility>
#include <concepts>

template<std::movable T, typename Allocator = std::allocator<T>>
class cqueue {
private:
#include "cqueue_iter.h"

public:
    using value_type = T;
    typedef size_t size_type;

    using pointer = value_type *;
    using const_pointer = const pointer;
    typedef ptrdiff_t difference_type;

    using reference = value_type &;
    using const_reference = const value_type &;

    using allocator_type = Allocator;

    using iterator = cqueue_iter<value_type>;
    using const_iterator = cqueue_iter<const value_type>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    using const_alloc_ref_t = const allocator_type &;
    using alloc_traits = std::allocator_traits<allocator_type>;

public:
    static constexpr auto max_capacity() noexcept { return MAX_CAPACITY; }

    constexpr explicit cqueue(size_type capacity = 0,
        const_alloc_ref_t alloc = Allocator());

    constexpr cqueue(const cqueue &other) :
        cqueue{ other, alloc_traits::
        select_on_container_copy_construction(other.get_allocator()) } {}
    constexpr cqueue(const cqueue &other, const_alloc_ref_t alloc);

    constexpr cqueue(cqueue &&other) noexcept { this->swap(other); }
    constexpr cqueue(cqueue &&other, const_alloc_ref_t alloc);

    ~cqueue() noexcept { reset(); };

    constexpr cqueue &operator=(const cqueue &other);
    constexpr cqueue &operator=(cqueue &&other) noexcept {
        this->swap(other); return *this;
    }

    constexpr allocator_type get_allocator() const noexcept { return _alloc; }
    constexpr auto capacity() const noexcept {
        return _capacity == MAX_CAPACITY ? 0 : _capacity;
    }
    constexpr auto size() const noexcept { return _length; }
    constexpr auto reserved() const noexcept { return _reserved; }
    [[nodiscard]] constexpr bool empty() const noexcept {
        return _length == 0;
    }
    [[nodiscard]] constexpr bool full() const noexcept {
        return size() == _capacity;
    }

    constexpr const_reference front() const { return operator[](0); }
    constexpr reference front() { return operator[](0); }
    constexpr const_reference back() const { return operator[](_length - 1); }
    constexpr reference back() { return operator[](_length - 1); }

    template <class... Args>
    constexpr reference emplace_back(Args&&... args);
    template <class... Args>
    constexpr reference emplace_front(Args&&... args);
    template <class... Args>
    constexpr reference emplace(Args&&... args) {
        return emplace_back(std::forward<Args>(args)...);
    }

    constexpr void push_back(const T &val);
    constexpr void push_back(T &&val);
    constexpr void push_front(const T &val);
    constexpr void push_front(T &&val);
    constexpr void push(const T &val) { return push_back(val); }
    constexpr void push(T &&val) { return push_back(std::move(val)); }

    constexpr value_type pop_front();
    constexpr value_type pop_back();
    constexpr value_type pop() { return pop_front(); }

    constexpr reference operator[](size_type n) {
        return _data[getCheckedIndex(n)];
    }
    constexpr const_reference operator[](size_type n) const {
        return _data[getCheckedIndex(n)];
    }

    constexpr iterator begin() noexcept {
        return iterator(this, 0);
    }
    constexpr iterator end() noexcept {
        return iterator(this, static_cast<difference_type>(size()));
    }
    constexpr const_iterator begin() const noexcept {
        return const_iterator(this, 0);
    }
    constexpr const_iterator end() const noexcept {
        return const_iterator(this, static_cast<difference_type>(size()));
    }
    constexpr const_iterator cbegin() const noexcept {
        return const_iterator(this, 0);
    }
    constexpr const_iterator cend() const noexcept {
        return const_iterator(this, static_cast<difference_type>(size()));
    }

    constexpr reverse_iterator rbegin() noexcept {
        return std::make_reverse_iterator(end());
    }
    constexpr reverse_iterator rend() noexcept {
        return std::make_reverse_iterator(begin());
    }
    constexpr const_reverse_iterator rbegin() const noexcept {
        return std::make_reverse_iterator(end());
    }
    constexpr const_reverse_iterator rend() const noexcept {
        return std::make_reverse_iterator(begin());
    }
    constexpr const_reverse_iterator crbegin() const noexcept {
        return std::make_reverse_iterator(end());
    }
    constexpr const_reverse_iterator crend() const noexcept {
        return std::make_reverse_iterator(begin());
    }

    constexpr void swap(cqueue &other) noexcept;

    constexpr void reserve(size_type n);
    constexpr void shrink_to_fit();

    void clear() noexcept;

private:
    static constexpr size_type GROWTH_FACTOR = 2;
    static constexpr size_type MIN_ALLOCATE = 8;
    static constexpr size_type MAX_CAPACITY =
        std::numeric_limits<difference_type>::max();

    [[no_unique_address]] allocator_type _alloc{};
    pointer _data = nullptr;

    size_type _reserved = 0, _capacity = MAX_CAPACITY;
    size_type _front = 0, _length = 0;

    constexpr auto getCheckedIndex(size_type pos) const noexcept(false);
    constexpr auto getUncheckedIndex(size_type pos) const noexcept;
    constexpr auto getNewMemoryLength(size_type n) const noexcept;

    void resize(size_type len);
    constexpr void resizeIfRequired(size_type n);

    void reset() noexcept;
};

#include "cqueue_impl.h"