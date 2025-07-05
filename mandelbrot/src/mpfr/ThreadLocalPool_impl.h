#define _CLASS_TEMPLATE template<typename T, size_t PoolSize>
#define _CLASS_NAME ThreadLocalPool<T, PoolSize>

_CLASS_TEMPLATE
T &_CLASS_NAME::getNextItem() {
    size_t &index = _getThreadIndex();
    T(&pool)[PoolSize] = _getThreadPool();

    T &item = pool[index];
    index = (index + 1) % PoolSize;

    return item;
}

_CLASS_TEMPLATE
T(&_CLASS_NAME::_getThreadPool())[PoolSize] {
    thread_local static T pool[PoolSize];
    return pool;
}

_CLASS_TEMPLATE
size_t &_CLASS_NAME::_getThreadIndex() {
    thread_local static size_t index = 0;
    return index;
}

#undef _CLASS_TEMPLATE
#undef _CLASS_NAME
