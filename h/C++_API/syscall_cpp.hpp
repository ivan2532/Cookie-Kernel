#ifndef _syscall_cpp_
#define _syscall_cpp_

#include "../C_API/syscall_c.hpp"

void* operator new (size_t size);
void* operator new[] (size_t size);
void* operator new (size_t size, void* ptr);

void operator delete (void* ptr);
void operator delete[] (void* ptr);

class Thread
{
public:
    Thread(void (*body)(void*), void* arg);
    virtual ~Thread();
    int start();
    void join();
    static void dispatch();
    static int sleep(time_t);

protected:
    Thread();
    virtual void run() { }

private:
    static void runWrapper(void* args);

    thread_t myHandle;
    void (*body)(void*);
    void* arg;
};


#endif
