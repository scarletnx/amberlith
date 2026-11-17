// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The module's header (docs/Mod.md), as a game's dynamic linker reads one of every module the
// loader put in the process: the word after the first instruction is where MOD0 is, and MOD0
// says where the dynamic section, the zeroed data and the linker's own record of the module are.
// The linker then binds the module's imports and calls its DT_INIT, EdenprobeModInit.

    .section .crt0, "ax", %progbits
    .global _start
    .hidden _start
    .balign 4
_start:
    // Never an entry point: the process starts in the game's own first module.
    brk #0x3e8
    .word EdenprobeModHeader - _start

    .section .rodata.EdenprobeModHeader, "a", %progbits
    .balign 4
EdenprobeModHeader:
    .ascii "MOD0"
    .word _DYNAMIC - EdenprobeModHeader
    .word __bss_start__ - EdenprobeModHeader
    .word __bss_end__ - EdenprobeModHeader
    .word __eh_frame_hdr_start - EdenprobeModHeader
    .word __eh_frame_hdr_end - EdenprobeModHeader
    .word EdenprobeModObject - EdenprobeModHeader

    // The linker's record of the module, which it writes here: 0xD0 bytes in libnx's own start,
    // and room to spare for a linker of another SDK.
    .section .bss.EdenprobeModObject, "aw", %nobits
    .balign 16
EdenprobeModObject:
    .space 0x200
