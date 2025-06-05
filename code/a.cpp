#include "ndb.h"
#include <thread>
#include <vector>
#include <random>
#include <cassert>
#include <chrono>

const int THREAD_NUM = 128;
const int KV_PER_THREAD = 100000;
const int VALUE_SIZE = 128;
const char* DBFILE = "test.ndb";

ndb db;

std::string generate_key(int tid, int i) {
    char buf[64];
    snprintf(buf, sizeof(buf), "key-%02d-%05d", tid, i);
    return std::string(buf);
}

std::string generate_value(int tid, int i) {
    std::string val = "VAL:" + std::to_string(tid) + ":" + std::to_string(i);
    while (val.size() < VALUE_SIZE) val += '.';
    return val;
}

void writer_thread(int tid) {
    for (int i = 0; i < KV_PER_THREAD; ++i) {
        std::string key = generate_key(tid, i);
        std::string val = generate_value(tid, i);
        char* ptr = (char*)ndb_create(&db, key.c_str(), VALUE_SIZE);
        assert(ptr); // 插入失败
        memcpy(ptr, val.data(), VALUE_SIZE);
    }
}

void reader_thread(int tid) {
    for (int i = 0; i < KV_PER_THREAD; ++i) {
        std::string key = generate_key(tid, i);
        std::string expected = generate_value(tid, i);
        char* ptr = (char*)ndb_create(&db, key.c_str(), 0); // read-only
        if (!ptr) {
            fprintf(stderr, "[FAIL] Key not found: %s\n", key.c_str());
            assert(false);
        } else if (std::string(ptr, VALUE_SIZE) != expected) {
            fprintf(stderr, "[FAIL] Value mismatch for %s\n", key.c_str());
            assert(false);
        }
    }
}

int main() {
    printf("Initializing NDB...\n");
    assert(!ndb_init(&db, DBFILE, 32,0x100000000)); // 使用 32 字节 key 区
    printf("Start multi-threaded insert (%d threads × %d keys)...\n", THREAD_NUM, KV_PER_THREAD);
    
    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_NUM; ++i) {
        threads.emplace_back(writer_thread, i);
    }
    for (auto& t : threads) t.join();
    threads.clear();

    auto mid = std::chrono::steady_clock::now();
    printf("Insert done in %.3f sec\n", 
           std::chrono::duration<double>(mid - start).count());

    printf("Start multi-threaded read check...\n");

    for (int i = 0; i < THREAD_NUM; ++i) {
        threads.emplace_back(reader_thread, i);
    }
    for (auto& t : threads) t.join();

    auto end = std::chrono::steady_clock::now();
    printf("Read check done in %.3f sec\n", 
           std::chrono::duration<double>(end - mid).count());

    ndb_free(&db);
    printf("✅ All tests passed.\n");

    return 0;
}
