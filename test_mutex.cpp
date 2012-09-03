// Usage examples:
//
//    test_mutex benaphore 4   # run test_mutex with libdispatch benaphore, 4 threads
//    test_mutex mutex 2       # run test_mutex with pthreads mutex, 2 threads
//    test_mutex mutex2 8      # run test_mutex with hybrid mutex, 8 threads

// Compilation:
//
//    g++ test_mutex.cpp -o test_mutex -Wall -Wextra -Werror -ansi -pedantic -O3
//
// Add -DNOCHECKS=1 to disable error checking.

#include <dispatch/dispatch.h>
#include <libkern/osatomic.h>
#include <pthread.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include <stdint.h>

// Prehistoric error checking for brevity
#if defined(NOCHECKS)
#   define CHECK(condition) (void)(condition)
#else
#   define CHECK(condition) \
        do \
        { \
            if (!(condition)) \
            { \
                std::cerr << "Failure: " << #condition << " (line " << __LINE__ << ")\n"; \
                std::abort(); \
            } \
        } \
        while (false)
#endif

class mutex
{
    public:
        mutex() { CHECK( pthread_mutex_init(&m, 0) == 0 ); }
        ~mutex() { CHECK( pthread_mutex_destroy(&m) == 0 ); }

        void lock() { CHECK( pthread_mutex_lock(&m) == 0 ); }
        void unlock() { CHECK( pthread_mutex_unlock(&m) == 0 ); }

    private:
        pthread_mutex_t m;
};

class benaphore
{
    public:
        benaphore() : count(0) { CHECK( sema = dispatch_semaphore_create(0) ); } // initial count is 0
        ~benaphore() { dispatch_release(sema); }

        void lock()
        {
            // DISPATCH_TIME_FOREVER is unsigned long long (not in ISO C++98/03).
            // Define our own equivalent.
            const uint64_t DISPATCH_TIME_FOREVER_u64 = ~uint64_t(0);

            if (OSAtomicIncrement32Barrier(&count) > 1) // if (++count > 1)
                CHECK( dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER_u64) == 0 ); // wait for unlock
        }

        void unlock()
        {
            if (OSAtomicDecrement32Barrier(&count) > 0) // if (--count > 0)
                dispatch_semaphore_signal(sema); // release a waiting thread
        }

    private:
        int32_t count;
        dispatch_semaphore_t sema;
};

class mutex2
{
    public:
        mutex2() : count(0) { CHECK( sema = dispatch_semaphore_create(0) ); } // initial count is 0
        ~mutex2() { dispatch_release(sema); }

        void lock()
        {
            for (unsigned spins = 0; spins != 5000; ++spins)
            {
                if (OSAtomicCompareAndSwap32Barrier(0, 1, &count))
                    return;

                sched_yield();
            }

            // DISPATCH_TIME_FOREVER is unsigned long long (not in ISO C++98/03).
            // Define our own equivalent.
            const uint64_t DISPATCH_TIME_FOREVER_u64 = ~uint64_t(0);

            if (OSAtomicIncrement32Barrier(&count) > 1) // if (++count > 1)
                CHECK( dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER_u64) == 0 ); // wait for unlock
        }

        void unlock()
        {
            if (OSAtomicDecrement32Barrier(&count) > 0) // if (--count > 0)
                dispatch_semaphore_signal(sema); // release a waiting thread
        }

    private:
        int32_t count;
        dispatch_semaphore_t sema;
};

template<typename Mutex>
struct shared_stuff
{
    shared_stuff(uint32_t increments) : 
        increments(increments),
        total(0) 
    { 
    }

    const uint32_t increments;

    char cache_line_separation1[64]; // put the mutex on its own cache line
    Mutex mtx;
    char cache_line_separation2[64]; // put the mutex on its own cache line

    uint32_t total;
};

template<typename Mutex>
void *thread_body(void *opaque_arg)
{
    CHECK( opaque_arg != 0 );
    shared_stuff<Mutex> &stuff = *static_cast<shared_stuff<Mutex> *>(opaque_arg);

    for (uint32_t i = 0; i != stuff.increments; ++i)
    {
        stuff.mtx.lock();
        ++stuff.total;
        stuff.mtx.unlock();
    }

    return 0;
}

template<typename Mutex>
void test_mutex(unsigned num_threads)
{
    const uint32_t increments = 20 * 1000 * 1000;

    std::cout << "Running test_mutex with " << num_threads << " threads\n";
    std::cout << "Increments in each thread: " << increments << "\n";

    shared_stuff<Mutex> stuff(increments);

    std::vector<pthread_t> threads;
    threads.reserve(num_threads);

    for (unsigned t = 0; t != num_threads; ++t)
    {
        pthread_t id;
        CHECK( pthread_create(&id, 0, &thread_body<Mutex>, &stuff) == 0 );
        threads.push_back(id);
    }

    for (unsigned t = 0; t != num_threads; ++t)
    {
        void *retval = 0;
        CHECK( pthread_join(threads[t], &retval) == 0 );
    }
        
    std::cerr << "expected: " << (num_threads * increments) << '\n';
    std::cerr << "actual:   " << stuff.total << '\n';
}

int main(int argc, char **argv)
{
    if (argc != 3) 
        return 1;
    
    unsigned num_threads = std::atoi(argv[2]);
    if (num_threads == 0 || num_threads > 32)
        return 1;

    if (std::strcmp(argv[1], "benaphore") == 0)
        test_mutex<benaphore>(num_threads);
    else if (std::strcmp(argv[1], "mutex") == 0)
        test_mutex<mutex>(num_threads);
    else if (std::strcmp(argv[1], "mutex2") == 0)
        test_mutex<mutex2>(num_threads);
    else
        return 1;

    return 0;
}
