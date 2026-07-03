#ifndef I_THREAD_LOCK_H
#define I_THREAD_LOCK_H

// interfaz para aislar 
class IThreadLock {
public:
    virtual ~IThreadLock() {}
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

// Un candado "vacío" 
class DummyLock : public IThreadLock {
public:
    void lock() override {}
    void unlock() override {}
};

#endif