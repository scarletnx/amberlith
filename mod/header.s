// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The module's header (docs/Mod.md), as a game's dynamic linker reads one of every module the
// loader put in the process: the word after the first instruction is where MOD0 is, and MOD0
// says where the dynamic section, the zeroed data and the linker's own record of the module are.
// The linker then binds the module's imports and calls its DT_INIT, AmberlithModInit.

    .section .crt0, "ax", %progbits
    .global _start
    .hidden _start
    .balign 4
_start:
    // Never an entry point: the process starts in the game's own first module.
    brk #0x3e8
    .word AmberlithModHeader - _start

    .section .rodata.AmberlithModHeader, "a", %progbits
    .balign 4
AmberlithModHeader:
    .ascii "MOD0"
    .word _DYNAMIC - AmberlithModHeader
    .word __bss_start__ - AmberlithModHeader
    .word __bss_end__ - AmberlithModHeader
    .word __eh_frame_hdr_start - AmberlithModHeader
    .word __eh_frame_hdr_end - AmberlithModHeader
    .word AmberlithModObject - AmberlithModHeader

    // The linker's record of the module, which it writes here: 0xD0 bytes in libnx's own start,
    // and room to spare for a linker of another SDK.
    .section .bss.AmberlithModObject, "aw", %nobits
    .balign 16
AmberlithModObject:
    .space 0x200
