; Windows ARM64 coroutine context switch
        AREA    |.text|, CODE, READONLY

        EXPORT  tb_context_make
        EXPORT  tb_context_jump
        IMPORT  ExitProcess

; -----------------------------------------------------------------------------
; tb_context_t tb_context_make(char* stackdata, size_t stacksize, tb_context_func_t func);
;   x0 = stackdata
;   x1 = stacksize
;   x2 = func
; -----------------------------------------------------------------------------
tb_context_make PROC
    add     x0, x0, x1              ; x0 = stackdata + stacksize
    bic     x0, x0, #0xF            ; 16-byte aligned

    ; reserve 176 bytes
    sub     x0, x0, #176

    ; context.pc = func (offset 96 = 0x60)
    str     x2, [x0, #96]

    ; context.lr = end_label (offset 88 = 0x58)
    adr     x1, end_label
    str     x1, [x0, #88]

    ; return context
    ret     x30

end_label
    mov     x0, #0
    bl      ExitProcess
    ENDP

; -----------------------------------------------------------------------------
; tb_context_from_t tb_context_jump(tb_context_t ctx, const void* priv);
;   x0 = (ctx)
;   x1 = (priv)
; return: x0 = from.ctx (old context)
;         x1 = from.priv (priv)
; -----------------------------------------------------------------------------
tb_context_jump PROC
    ; reserve 0xB0 bytes
    sub     sp, sp, #0xB0

    ; save x19 - x30
    stp     x19, x20, [sp, #0x00]
    stp     x21, x22, [sp, #0x10]
    stp     x23, x24, [sp, #0x20]
    stp     x25, x26, [sp, #0x30]
    stp     x27, x28, [sp, #0x40]
    stp     x29, x30, [sp, #0x50]
    str     x30, [sp, #0x60]

    ; save d8 - d15 (callee-saved, the low 64 bits of v8 - v15)
    stp     d8,  d9,  [sp, #0x70]
    stp     d10, d11, [sp, #0x80]
    stp     d12, d13, [sp, #0x90]
    stp     d14, d15, [sp, #0xA0]

    ; save old (sp) to x4
    mov     x4, sp

    ; jump to new context
    mov     sp, x0

    ; restore x19 - x30
    ldp     x19, x20, [sp, #0x00]
    ldp     x21, x22, [sp, #0x10]
    ldp     x23, x24, [sp, #0x20]
    ldp     x25, x26, [sp, #0x30]
    ldp     x27, x28, [sp, #0x40]
    ldp     x29, x30, [sp, #0x50]

    ; restore d8 - d15
    ldp     d8,  d9,  [sp, #0x70]
    ldp     d10, d11, [sp, #0x80]
    ldp     d12, d13, [sp, #0x90]
    ldp     d14, d15, [sp, #0xA0]

    ; set from-context to return：x0 = old context，x1 = priv
    mov     x0, x4

    ; load jump address (pc)
    ldr     x4, [sp, #0x60]

    ; restore stack
    add     sp, sp, #0xB0

    ; jump
    ret     x4

    ENDP

    END
 