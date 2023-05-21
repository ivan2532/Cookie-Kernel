# 1 "src/Kernel/contextSwitch.S"
# 1 "<built-in>"
# 1 "<command-line>"
# 31 "<command-line>"
# 1 "/usr/riscv64-linux-gnu/include/stdc-predef.h" 1 3
# 32 "<command-line>" 2
# 1 "src/Kernel/contextSwitch.S"
.global _ZN3TCB13contextSwitchEPNS_7ContextES1_PVb
.type _ZN3TCB13contextSwitchEPNS_7ContextES1_PVb, @function
_ZN3TCB13contextSwitchEPNS_7ContextES1_PVb:
    # a0 => &old->context
    # a1 => &running->context
    # a2 => &kernelLock

    # Skip saving context if we got nullptr as old context
    beqz a0, load

    # Save old thread's Context (ra and sp)
    sd ra, 0 * 8(a0) # 0*8(a0) is the old context's first field (uint64 ra)
    sd sp, 1 * 8(a0) # 1*8(a0) is the old context's second field (uint64 sp)

    # Load new thread's Context (ra and sp)
load:
    ld ra, 0 * 8(a1) # 0*8(a0) is the new context's first field (uint64 ra)
    ld sp, 1 * 8(a1) # 1*8(a0) is the new context's second field (uint64 sp)

    # Unlock
    sd x0, 0(a2)

    ret
