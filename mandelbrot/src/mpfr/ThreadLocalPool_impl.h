template<typename T, size_t PoolSize>
T &ThreadLocalPool<T, PoolSize>::getNextItem() {
    size_t &index = _getThreadIndex();
    T(&pool)[PoolSize] = _getThreadPool();

    T &item = pool[index];
    index = (index + 1) % PoolSize;

    return item;
}

template<typename T, size_t PoolSize>
T(&ThreadLocalPool<T, PoolSize>::_getThreadPool())[PoolSize] {
    thread_local static T pool[PoolSize];
    return pool;
}

template<typename T, size_t PoolSize>
size_t &ThreadLocalPool<T, PoolSize>::_getThreadIndex() {
    thread_local static size_t index = 0;
    return index;
}