#ifndef I_THREAD_LOCK_H
#define I_THREAD_LOCK_H

/**
 * @class IThreadLock
 * @brief Interface providing a generic mutex abstraction to handle shared resource safety.
 * * Define a synchronization contract to protect hardware buses or shared memory 
 * across different execution threads or tasks.
 */
class IThreadLock {
public:
    /**
     * @brief Virtual destructor to ensure proper cleanup of deriving implementation classes.
     */
    virtual ~IThreadLock() {}

    /**
     * @brief Blocks the calling thread until the lock becomes available for exclusive access.
     */
    virtual void lock() = 0;

    /**
     * @brief Releases the lock, allowing other waiting threads to claim exclusive access.
     */
    virtual void unlock() = 0;
};

/**
 * @class DummyLock
 * @brief A non-blocking "Null Object" implementation of the IThreadLock interface.
 * * Used as a safe fallback mechanism when thread safety or hardware isolation 
 * is not required by the system architecture, avoiding null pointer checks.
 */
class DummyLock : public IThreadLock {
public:
    void lock() override {}
    void unlock() override {}
};

#endif