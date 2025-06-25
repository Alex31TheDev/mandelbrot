#pragma once

template<typename T, size_t PoolSize>
class ThreadLocalPool {
    static_assert(PoolSize > 0, "Pool size must be greater than zero");

public:
    T &getNextItem();

private:
    static T(&_getThreadPool())[PoolSize];
    static size_t &_getThreadIndex();
};

#include "ThreadLocalPool_impl.h"
