#include "../../h/Kernel/Kernel.hpp"
#include "../../h/Kernel/TCB.hpp"
#include "../../h/Kernel/SCB.hpp"

uint64 Kernel::oldTrapHandler = 0;

Kernel::SystemCallHandler Kernel::systemCallHandlers[SYSTEM_CALL_HANDLERS_SIZE] = {};

volatile KernelDeque<char> Kernel::inputQueue;
SCB* volatile Kernel::inputEmptySemaphore;
SCB* volatile Kernel::inputFullSemaphore;

volatile KernelDeque<char> Kernel::outputQueue;
SCB* volatile Kernel::outputEmptySemaphore;
SCB* volatile Kernel::outputFullSemaphore;

SCB* volatile Kernel::outputControllerReadySemaphore;

void Kernel::initialize()
{
    initializeSystemCallHandlers();

    oldTrapHandler = readStvec();
    writeStvec((uint64)&supervisorTrap + 1);

    initializeSystemThreads();
    initializeIO();
    initializeUserThread();
}

void Kernel::initializeSystemThreads()
{
    // When we create the main thread (specific case when body = nullptr) we don't put it in the Scheduler,
    // it will gain it's returning context once it gives the processor to another thread
    TCB::mainThread = TCB::createThread(nullptr, nullptr, nullptr, true);

    // Create idle thread
    auto idleThreadStack = MemoryAllocator::alloc(DEFAULT_STACK_SIZE + STACK_CONTEXT_EXTENSION);
    TCB::idleThread = TCB::createThread
    (
        TCB::idleThreadBody,
        nullptr,
        idleThreadStack,
        true
    );
}

void Kernel::initializeIO()
{
    inputEmptySemaphore = static_cast<SCB*>(MemoryAllocator::alloc(sizeof(SCB)));
    inputFullSemaphore = static_cast<SCB*>(MemoryAllocator::alloc(sizeof(SCB)));
    outputEmptySemaphore = static_cast<SCB*>(MemoryAllocator::alloc(sizeof(SCB)));
    outputFullSemaphore = static_cast<SCB*>(MemoryAllocator::alloc(sizeof(SCB)));
    outputControllerReadySemaphore = static_cast<SCB*>(MemoryAllocator::alloc(sizeof(SCB)));

    new (inputEmptySemaphore) volatile SCB(INPUT_BUFFER_SIZE);
    new (inputFullSemaphore) volatile SCB(0);
    new (outputEmptySemaphore) volatile SCB(OUTPUT_BUFFER_SIZE);
    new (outputFullSemaphore) volatile SCB(0);
    new (outputControllerReadySemaphore) volatile SCB(0, true);

    // Create io thread
    auto outputThreadStack = MemoryAllocator::alloc(DEFAULT_STACK_SIZE + STACK_CONTEXT_EXTENSION);
    TCB::outputThread = TCB::createThread
    (
        TCB::outputThreadBody,
        nullptr,
        outputThreadStack,
        true
    );

    // Enable interrupts
    maskSetSstatus(SSTATUS_SIE);
    thread_dispatch();
}

void Kernel::initializeUserThread()
{
    extern void userMain();

    auto userThreadStack = MemoryAllocator::alloc(DEFAULT_STACK_SIZE + STACK_CONTEXT_EXTENSION);
    TCB::userThread = TCB::createThread([](void*) { userMain(); }, nullptr, userThreadStack);
    thread_dispatch();

    // Wait for user thread to finish
    TCB::running->waitForThread(TCB::userThread);
}

void Kernel::dispose()
{
    delete TCB::mainThread;
    delete TCB::idleThread;
    delete TCB::outputThread;
    writeStvec(oldTrapHandler);
}

void Kernel::returnFromSystemCall()
{
    maskClearSstatus(SSTATUS_SPP);
    __asm__ volatile ("csrw sepc, ra");
    __asm__ volatile ("sret");
}

void Kernel::handleTimerTrap()
{
    // Clear interrupt pending bit
    maskClearSip(SIP_SSIP);
    if(TCB::running == nullptr) return;

    for(auto it = TCB::allThreads.head; it != nullptr; it = it->next)
    {
        if(it->data->m_SleepCounter == 0) continue;
        if(--(it->data->m_SleepCounter) == 0)
        {
            Scheduler::put(it->data);
        }
    }

    TCB::timeSliceCounter++;
    if(TCB::timeSliceCounter >= TCB::running->m_TimeSlice)
    {
        auto volatile sepc = readSepc();
        auto volatile sstatus = readSstatus();

        TCB::timeSliceCounter = 0;
        TCB::dispatch();

        // Restore important supervisor registers
        writeSstatus(sstatus);
        writeSepc(sepc);
    }
}

void Kernel::handleExternalTrap()
{
    // Clear interrupt pending bit
    maskClearSip(SIP_SSIP);
    auto interruptId = plic_claim();

    // Check if the console generated an interrupt
    if(interruptId == CONSOLE_IRQ)
    {
        auto pStatus = *((char*)CONSOLE_STATUS);

        // Signal that the controller is ready to print
        if(pStatus & CONSOLE_TX_STATUS_BIT)
        {
            outputControllerReadySemaphore->signal();
        }

        // Read from the controller
        if(pStatus & CONSOLE_RX_STATUS_BIT)
        {
            auto pInData = *((char*)CONSOLE_RX_DATA);
            inputEmptySemaphore->wait();
            if(pInData == '\r') pInData = '\n';
            inputQueue.addLast(pInData);
            inputFullSemaphore->signal();
        }
    }

    plic_complete(interruptId);
}

void Kernel::handleEcallTrap()
{
    // Clear interrupt pending bit
    maskClearSip(SIP_SSIP);

    // Ecall will have a sepc that points back to ecall, so we want to return to the
    // instruction after that ecall
    constexpr auto EcallInstructionSize = 4;

    // Save important supervisor registers on the stack!
    auto volatile sepc = readSepc() + EcallInstructionSize;
    auto volatile sstatus = readSstatus();

    auto volatile scause = readScause();
    if(scause == SCAUSE_ECALL_FROM_SUPERVISOR_MODE) TCB::dispatch();
    else if(scause == SCAUSE_ECALL_FROM_USER_MODE)
    {
        uint64 volatile systemCallCode;
        __asm__ volatile ("mv %[outCode], a0" : [outCode] "=r" (systemCallCode));
        handleSystemCalls(systemCallCode, scause);
    }
    else handleUnknownTrapCause(scause);

    // Restore important supervisor registers
    writeSstatus(sstatus);
    writeSepc(sepc);
}

[[noreturn]] void Kernel::handleUnknownTrapCause(uint64 scause)
{
    KernelPrinter::printString("\nscause: ");
    KernelPrinter::printNumber(scause, 16);

    auto volatile sepc = readSepc();
    KernelPrinter::printString("\nsepc: ");
    KernelPrinter::printNumber(sepc, 16);

    auto volatile stval = readStval();
    KernelPrinter::printString("\nstval: ");
    KernelPrinter::printNumber(stval, 16);

    while(true) TCB::dispatch();
}

void Kernel::initializeSystemCallHandlers()
{
    systemCallHandlers[SYS_CALL_MEM_ALLOC] = handleMemAlloc;
    systemCallHandlers[SYS_CALL_MEM_FREE] = handleMemFree;
    systemCallHandlers[SYS_CALL_THREAD_CREATE] = handleThreadCreate;
    systemCallHandlers[SYS_CALL_THREAD_EXIT] = handleThreadExit;
    systemCallHandlers[SYS_CALL_THREAD_DISPATCH] = handleThreadDispatch;
    systemCallHandlers[SYS_CALL_THREAD_JOIN] = handleThreadJoin;
    systemCallHandlers[SYS_CALL_SEM_OPEN] = handleSemaphoreOpen;
    systemCallHandlers[SYS_CALL_SEM_CLOSE] = handleSemaphoreClose;
    systemCallHandlers[SYS_CALL_SEM_WAIT] = handleSemaphoreWait;
    systemCallHandlers[SYS_CALL_SEM_SIGNAL] = handleSemaphoreSignal;
    systemCallHandlers[SYS_CALL_TIME_SLEEP] = handleTimeSleep;
    systemCallHandlers[SYS_CALL_GET_CHAR] = handleGetChar;
    systemCallHandlers[SYS_CALL_PUT_CHAR] = handlePutChar;
}

void Kernel::handleSystemCalls(uint64 systemCallCode, uint64 scause)
{
    if(systemCallCode < 0 ||
       systemCallCode >= SYSTEM_CALL_HANDLERS_SIZE ||
       systemCallHandlers[systemCallCode] == nullptr)
    {
        handleUnknownTrapCause(scause);
    }
    else systemCallHandlers[systemCallCode]();
}

void Kernel::handleMemAlloc()
{
    // Get arguments
    size_t volatile sizeArg;
    __asm__ volatile ("mv %[outSize], a1" : [outSize] "=r" (sizeArg));

    auto volatile returnValue = MemoryAllocator::alloc(sizeArg);

    // Store result in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleMemFree()
{
    void* volatile ptrArg;

    // Get arguments
    __asm__ volatile ("mv %[outPtr], a1" : [outPtr] "=r" (ptrArg));

    auto volatile returnValue = MemoryAllocator::free(ptrArg);

    // Store result in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleThreadCreate()
{
    TCB::Body volatile routine;
    void* volatile args;
    void* volatile stack;

    // Save A1 (handle) to A7, A1 will be overwritten by createThread
    __asm__ volatile ("mv a7, a1");

    // Get other arguments
    __asm__ volatile ("mv %[outRoutine], a2" : [outRoutine] "=r" (routine));
    __asm__ volatile ("mv %[outArgs], a3" : [outArgs] "=r" (args));
    __asm__ volatile ("mv %[outStack], a6" : [outStack] "=r" (stack));

    auto newTCB = TCB::createThread(routine, args, stack);

    // Get handle
    TCB** volatile handle;
    __asm__ volatile ("mv %[inHandle], a7" : [inHandle] "=r" (handle));

    *handle = newTCB;
    auto returnValue = (*handle == nullptr ? -1 : 0);

    // Store results in A0 and A1
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleThreadExit()
{
    auto returnValue = TCB::deleteThread(TCB::running);

    // Store result in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleThreadDispatch()
{
    TCB::dispatch();
}

void Kernel::handleThreadJoin()
{
    TCB* volatile handle;

    // Get arguments
    __asm__ volatile ("mv %[outHandle], a1" : [outHandle] "=r" (handle));

    TCB::running->waitForThread(handle);
}

void Kernel::handleSemaphoreOpen()
{
    // Save handle to A7, it will be overwritten by alloc
    __asm__ volatile ("mv a7, a1");

    auto newSCB = static_cast<SCB*>(MemoryAllocator::alloc(sizeof(SCB)));

    SCB** volatile handle;
    unsigned volatile init;

    // Get arguments
    __asm__ volatile ("mv %[outHandle], a7" : [outHandle] "=r" (handle));
    __asm__ volatile ("mv %[outInit], a2" : [outInit] "=r" (init));

    *handle = new (newSCB) SCB(init);

    auto returnValue = (*handle == nullptr ? -1 : 0);

    // Store results in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleSemaphoreClose()
{
    // Move handle to A7, it can be overwritten by signal()
    __asm__ volatile ("mv a7, a1");

    SCB* volatile handle;

    // Get arguments
    __asm__ volatile ("mv %[outHandle], a7" : [outHandle] "=r" (handle));

    handle->~SCB();
    auto returnValue = MemoryAllocator::free(handle);

    // Store results in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleSemaphoreWait()
{
    // Move id to A7, it can be overwritten by signal()
    __asm__ volatile ("mv a7, a1");

    SCB* volatile id;

    // Get arguments
    __asm__ volatile ("mv %[outId], a7" : [outId] "=r" (id));

    id->wait();
    auto returnValue = 0;

    // Store results in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleSemaphoreSignal()
{
    // Move id to A7, it can be overwritten by signal()
    __asm__ volatile ("mv a7, a1");

    SCB* volatile id;

    // Get arguments
    __asm__ volatile ("mv %[outId], a7" : [outId] "=r" (id));

    id->signal();
    auto returnValue = 0;

    // Store results in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleTimeSleep()
{
    time_t volatile time;

    // Get arguments
    __asm__ volatile ("mv %[outTime], a1" : [outTime] "=r" (time));

    auto returnValue = TCB::sleep(time);

    // Store results in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handleGetChar()
{
    auto returnValue = getCharFromInputBuffer();

    // Store results in A0
    __asm__ volatile ("mv a0, %[inReturnValue]" : : [inReturnValue] "r" (returnValue));
}

void Kernel::handlePutChar()
{
    char volatile outputChar;

    // Get arguments
    __asm__ volatile ("mv %[outChar], a1" : [outChar] "=r" (outputChar));

    addCharToOutputBuffer(outputChar);
}

char Kernel::getCharFromInputBuffer()
{
    Kernel::inputFullSemaphore->wait();
    auto inputChar = inputQueue.removeFirst();
    Kernel::inputEmptySemaphore->signal();

    return inputChar;
}

void Kernel::addCharToOutputBuffer(char outputChar)
{
    outputEmptySemaphore->wait();
    outputQueue.addLast(outputChar);
    outputFullSemaphore->signal();
}
