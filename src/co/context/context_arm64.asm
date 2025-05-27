    AREA |.text|, CODE, READONLY
    EXPORT tb_context_make
    EXPORT tb_context_jump
    IMPORT ExitProcess

tb_context_make PROC
    add x0, x0, x1
    and x0, x0, ~0xf
    sub x0, x0, #112
    str x2, [x0, #96]
    adr x1, __end
    str x1, [x0, #88]
    ret x30

__end
    mov x0, #0
    bl ExitProcess

    ENDP

tb_context_jump PROC
    sub sp, sp, #0x70
    stp x19, x20, [sp, #0x00]
    stp x21, x22, [sp, #0x10]
    stp x23, x24, [sp, #0x20]
    stp x25, x26, [sp, #0x30]
    stp x27, x28, [sp, #0x40]
    stp x29, x30, [sp, #0x50]
    str x30, [sp, #0x60]

    mov x4, sp
    mov sp, x0

    ldp x19, x20, [sp, #0x00]
    ldp x21, x22, [sp, #0x10]
    ldp x23, x24, [sp, #0x20]
    ldp x25, x26, [sp, #0x30]
    ldp x27, x28, [sp, #0x40]
    ldp x29, x30, [sp, #0x50]

    mov x0, x4
    ldr x4, [sp, #0x60]
    add sp, sp, #0x70
    ret x4

    ENDP

    END
