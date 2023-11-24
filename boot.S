.section ".text.boot"  // Make sure the linker puts this at the start of the kernel image

.global _start

// Entry point for the kernel. Registers:
// x0 -> 32 bit pointer to DTB in memory (primary core only) / 0 (secondary cores)
// x1 -> 0
// x2 -> 0
// x3 -> 0
// x4 -> 32 bit kernel entry point, _start location
_start:
    // Set stack to start below our code
    ldr     x5, =_start
    mov     sp, x5

    // Clean the BSS section
    ldr     x5, =__bss_start     // Start address
    ldr     w6, =__bss_size      // Size of the section
1:  cbz     w6, 2f               // Quit loop if zero
    str     xzr, [x5], #8
    sub     w6, w6, #1
    cbnz    w6, 1b               // Loop if non-zero

    // Jump to our main() routine in C (make sure it doesn't return)
2:  bl      main

// for failsafe, halt this core
halt:
    wfe
    b halt
