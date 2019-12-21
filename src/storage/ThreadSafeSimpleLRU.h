#ifndef AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
#define AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H

#include <map>
#include <mutex>
#include <string>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

/**
 * # SimpleLRU thread safe version
 *
 *
 */
class ThreadSafeSimplLRU : public SimpleLRU {
public:
    ThreadSafeSimplLRU(size_t max_size = 1024) : SimpleLRU(max_size) {}
    ~ThreadSafeSimplLRU() {}

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        _storage_mutex.lock();
        bool res = SimpleLRU::Put(key, value);
        _storage_mutex.unlock();

        return res;
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        _storage_mutex.lock();
        bool res = SimpleLRU::PutIfAbsent(key, value);
        _storage_mutex.unlock();

        return res;}

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        _storage_mutex.lock();
        bool res = SimpleLRU::Set(key, value);
        _storage_mutex.unlock();

        return res;
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override {
        _storage_mutex.lock();
        bool res = SimpleLRU::Delete(key);
        _storage_mutex.unlock();

        return res;

    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        _storage_mutex.lock();
        bool res = SimpleLRU::Get(key, value);
        _storage_mutex.unlock();

        return res;
    }

private:
    std::mutex _storage_mutex;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
