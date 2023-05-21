#ifndef _infinite_threads_test_hpp_
#define _infinite_threads_test_hpp_

#include "../C++_API/syscall_cpp.hpp"
#include "Riscv.hpp"
#include "print.hpp"

class InfiniteThreadA : public Thread
{
public:
    InfiniteThreadA()
    {

    }

    void run() override
    {
        while(true)
        {
            m_CounterA++;
            Riscv::lock();
            printString("Counter A: ");
            printInteger(m_CounterA);
            printString("\n");
            Riscv::unlock();
        }
    }

private:
    uint64 m_CounterA = 0;
};

class InfiniteThreadB : public Thread
{
public:
    InfiniteThreadB() = default;

    void run() override
    {
        while(true)
        {
            m_CounterB++;
            Riscv::lock();
            printString("Counter B: ");
            printInteger(m_CounterB);
            printString("\n");
            Riscv::unlock();
        }
    }

private:
    uint64 m_CounterB = 0;
};

class InfiniteThreadC : public Thread
{
public:
    InfiniteThreadC() = default;

    void run() override
    {
        while(true)
        {
            m_CounterC++;
            Riscv::lock();
            printString("Counter C: ");
            printInteger(m_CounterC);
            printString("\n");
            Riscv::unlock();
        }
    }

private:
    uint64 m_CounterC = 0;
};

#endif // _infinite_threads_test_hpp_