#include "../../h/C++_API/syscall_cpp.hpp"

void* operator new (size_t size)
{
    return mem_alloc(size);
}

void* operator new[] (size_t size)
{
    return mem_alloc(size);
}

void* operator new (size_t size, void* ptr)
{
    return ptr;
}

void operator delete (void* ptr)
{
    mem_free(ptr);
}

void operator delete[] (void* ptr)
{
    mem_free(ptr);
}

Thread::Thread(void (*body)(void *), void *arg)
    :
    body(body),
    arg(arg)
{
    // Can't create main thread, it gets created by the system
    if(body == nullptr) return;
    thread_create(&myHandle, body, arg);
}

Thread::~Thread()
{
}

int Thread::start()
{
    if(body != nullptr) return -1;

    body = &runWrapper;
    arg = this;

    return thread_create(&myHandle, body, arg);
}

void Thread::join()
{
    thread_join(myHandle);
}

void Thread::dispatch()
{
    thread_dispatch();
}

Thread::Thread()
    :
    body(nullptr)
{
}

void Thread::runWrapper(void *args)
{
    auto pThread = static_cast<Thread*>(args);
    pThread->run();
}

int Thread::sleep(time_t)
{
    return -1;
}
