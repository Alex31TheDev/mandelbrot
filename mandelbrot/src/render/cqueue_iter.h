#include <compare>
#include <type_traits>

template<typename U>
class cqueue_iter {
public:
    using iterator_category = std::random_access_iterator_tag;

    using value_type = U;
    typedef size_t size_type;

    using pointer = value_type *;
    typedef ptrdiff_t difference_type;

    using reference = value_type &;

private:
    using queue_t = std::conditional_t<
        std::is_const_v<value_type>,
        const cqueue, cqueue
    >;

public:
    explicit cqueue_iter(queue_t *other = nullptr,
        difference_type position = 0) : _queue{ other }, _pos{ position } {}
    cqueue_iter(const cqueue_iter<std::remove_const_t<value_type>>
        &other) requires std::is_const_v<value_type> :
        _queue{ other._queue }, _pos{ other._pos } {}

    cqueue_iter(const cqueue_iter<value_type> &other) = default;
    cqueue_iter &operator=(const cqueue_iter &other) = default;

    reference operator*() const {
        return _queue->operator[](cast(_pos));
    }
    pointer operator->() const {
        return &(_queue->operator[](cast(_pos)));
    }
    reference operator[](difference_type rhs) const {
        return _queue->operator[](cast(_pos + rhs));
    }

    auto operator<=>(const cqueue_iter &rhs) const {
        return _queue == rhs._queue ?
            _pos <=> rhs._pos : std::partial_ordering::unordered;
    }
    bool operator==(const cqueue_iter &rhs) const {
        return (*this <=> rhs) == 0;
    }

    cqueue_iter &operator++() { return *this += 1; }
    cqueue_iter &operator--() { return *this += -1; }

    [[nodiscard]] cqueue_iter operator++(int) {
        cqueue_iter tmp{ _queue, _pos };
        ++*this;
        return tmp;
    }
    [[nodiscard]] cqueue_iter operator--(int) {
        cqueue_iter tmp{ _queue, _pos };
        --*this;
        return tmp;
    }

    auto &operator+=(difference_type rhs) { _pos += rhs; return *this; }
    auto &operator-=(difference_type rhs) { _pos -= rhs; return *this; }

    auto operator+(difference_type rhs) const {
        return cqueue_iter{ _queue, _pos + rhs };
    }
    auto operator-(difference_type rhs) const {
        return cqueue_iter{ _queue, _pos - rhs };
    }

    friend cqueue_iter operator+(difference_type lhs,
        const cqueue_iter &rhs) {
        return cqueue_iter{ rhs._queue, lhs + rhs._pos };
    }
    friend cqueue_iter operator-(difference_type lhs,
        const cqueue_iter &rhs) {
        return cqueue_iter{ rhs._queue, lhs - rhs._pos };
    }

    auto operator-(const cqueue_iter &rhs) const {
        return _pos - rhs._pos;
    }

private:
    friend class cqueue_iter<std::add_const_t<value_type>>;

    queue_t *_queue = nullptr;
    difference_type _pos = 0;

    auto cast(difference_type n) const {
        return n < 0 ? _queue->size() : static_cast<size_type>(n);
    }
};