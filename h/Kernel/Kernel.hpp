#ifndef _Kernel_hpp_
#define _Kernel_hpp_

#include "../../lib/hw.h"
#include "../../h/Kernel/KernelDeque.hpp"
#include "../../h/Kernel/KernelPrinter.hpp"

class Kernel
{
    friend class TCB;
    friend class SCB;

public:
    static void initialize();
    static void dispose();

    static void returnFromSystemCall();

    static uint64 readScause();
    static void writeScause(uint64 scause);

    static uint64 readSepc();
    static void writeSepc(uint64 sepc);

    static uint64 readStvec();
    static void writeStvec(uint64 stvec);

    static uint64 readStval();
    static void writeStval(uint64 stval);

    enum BitMaskSip
    {
        SIP_SSIP = (1 << 1),
        SIP_STIP = (1 << 5),
        SIP_SEIP = (1 << 9),
    };

    static void maskSetSip(uint64 mask);
    static void maskClearSip(uint64 mask);
    static uint64 readSip();
    static void writeSip(uint64 sip);

    enum BitMaskSstatus
    {
        SSTATUS_SIE = (1 << 1),
        SSTATUS_SPIE = (1 << 5),
        SSTATUS_SPP = (1 << 8),
    };

    static void maskSetSstatus(uint64 mask);
    static void maskClearSstatus(uint64 mask);
    static uint64 readSstatus();
    static void writeSstatus(uint64 sstatus);

    inline static void lock() { Kernel::maskClearSstatus(Kernel::SSTATUS_SIE); }
    inline static void unlock() { Kernel::maskSetSstatus(Kernel::SSTATUS_SIE); }

    static char getCharFromInputBuffer();
    static void addCharToOutputBuffer(char outputChar);

private:
    static void initializeSystemThreads();
    static void initializeIO();
    static void initializeUserThread();

    static uint64 oldTrapHandler;
    static void supervisorTrap();
    static constexpr uint64 SCAUSE_ECALL_FROM_USER_MODE = 0x0000000000000008UL;
    static constexpr uint64 SCAUSE_ECALL_FROM_SUPERVISOR_MODE = 0x0000000000000009UL;

    static void handleEcallTrap();
    static void handleTimerTrap();
    static void handleExternalTrap();
    [[noreturn]] inline static void handleUnknownTrapCause(uint64 scause);

    typedef void (*SystemCallHandler)();
    static constexpr size_t SYSTEM_CALL_HANDLERS_SIZE = 0x42 + 1;
    static SystemCallHandler systemCallHandlers[];
    static void initializeSystemCallHandlers();

    inline static void handleSystemCalls(uint64 systemCallCode, uint64 scause);
    inline static void handleMemAlloc();
    inline static void handleMemFree();
    inline static void handleThreadCreate();
    inline static void handleThreadExit();
    inline static void handleThreadDispatch();
    inline static void handleThreadJoin();
    inline static void handleSemaphoreOpen();
    inline static void handleSemaphoreClose();
    inline static void handleSemaphoreWait();
    inline static void handleSemaphoreSignal();
    inline static void handleTimeSleep();
    inline static void handleGetChar();
    inline static void handlePutChar();

    static constexpr uint64 SYS_CALL_MEM_ALLOC = 0x01;
    static constexpr uint64 SYS_CALL_MEM_FREE = 0x02;
    static constexpr uint64 SYS_CALL_THREAD_CREATE = 0x11;
    static constexpr uint64 SYS_CALL_THREAD_EXIT = 0x12;
    static constexpr uint64 SYS_CALL_THREAD_DISPATCH = 0x13;
    static constexpr uint64 SYS_CALL_THREAD_JOIN = 0x14;
    static constexpr uint64 SYS_CALL_SEM_OPEN = 0x21;
    static constexpr uint64 SYS_CALL_SEM_CLOSE = 0x22;
    static constexpr uint64 SYS_CALL_SEM_WAIT = 0x23;
    static constexpr uint64 SYS_CALL_SEM_SIGNAL = 0x24;
    static constexpr uint64 SYS_CALL_TIME_SLEEP = 0x31;
    static constexpr uint64 SYS_CALL_GET_CHAR = 0x41;
    static constexpr uint64 SYS_CALL_PUT_CHAR = 0x42;

    static volatile KernelDeque<char> inputQueue;
    static constexpr uint16 INPUT_BUFFER_SIZE = 100;
    static SCB* volatile inputEmptySemaphore;
    static SCB* volatile inputFullSemaphore;

    static volatile KernelDeque<char> outputQueue;
    static constexpr uint16 OUTPUT_BUFFER_SIZE = 100;
    static SCB* volatile outputEmptySemaphore;
    static SCB* volatile outputFullSemaphore;

    static SCB* volatile outputControllerReadySemaphore;
};

inline uint64 Kernel::readScause()
{
    uint64 volatile scause;
    __asm__ volatile ("csrr %[scause], scause" : [scause] "=r"(scause));
    return scause;
}

inline void Kernel::writeScause(uint64 scause)
{
    __asm__ volatile ("csrw scause, %[scause]" : : [scause] "r"(scause));
}

inline uint64 Kernel::readSepc()
{
    uint64 volatile sepc;
    __asm__ volatile ("csrr %[sepc], sepc" : [sepc] "=r"(sepc));
    return sepc;
}

inline void Kernel::writeSepc(uint64 sepc)
{
    __asm__ volatile ("csrw sepc, %[sepc]" : : [sepc] "r"(sepc));
}

inline uint64 Kernel::readStvec()
{
    uint64 volatile stvec;
    __asm__ volatile ("csrr %[stvec], stvec" : [stvec] "=r"(stvec));
    return stvec;
}

inline void Kernel::writeStvec(uint64 stvec)
{
    __asm__ volatile ("csrw stvec, %[stvec]" : : [stvec] "r"(stvec));
}

inline uint64 Kernel::readStval()
{
    uint64 volatile stval;
    __asm__ volatile ("csrr %[stval], stval" : [stval] "=r"(stval));
    return stval;
}

inline void Kernel::writeStval(uint64 stval)
{
    __asm__ volatile ("csrw stval, %[stval]" : : [stval] "r"(stval));
}

inline void Kernel::maskSetSip(uint64 mask)
{
    __asm__ volatile ("csrs sip, %[mask]" : : [mask] "r"(mask));
}

inline void Kernel::maskClearSip(uint64 mask)
{
    __asm__ volatile ("csrc sip, %[mask]" : : [mask] "r"(mask));
}

inline uint64 Kernel::readSip()
{
    uint64 volatile sip;
    __asm__ volatile ("csrr %[sip], sip" : [sip] "=r"(sip));
    return sip;
}

inline void Kernel::writeSip(uint64 sip)
{
    __asm__ volatile ("csrw sip, %[sip]" : : [sip] "r"(sip));
}

inline void Kernel::maskSetSstatus(uint64 mask)
{
    __asm__ volatile ("csrs sstatus, %[mask]" : : [mask] "r"(mask));
}

inline void Kernel::maskClearSstatus(uint64 mask)
{
    __asm__ volatile ("csrc sstatus, %[mask]" : : [mask] "r"(mask));
}

inline uint64 Kernel::readSstatus()
{
    uint64 volatile sstatus;
    __asm__ volatile ("csrr %[sstatus], sstatus" : [sstatus] "=r"(sstatus));
    return sstatus;
}

inline void Kernel::writeSstatus(uint64 sstatus)
{
    __asm__ volatile ("csrw sstatus, %[sstatus]" : : [sstatus] "r"(sstatus));
}

#endif //_Kernel_hpp_
