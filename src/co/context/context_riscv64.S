/* make context
 *
 * @param a0    the stack data
 * @param a1    the stack size
 * @param a2    the entry function
 *
 * @return a0   the context pointer
 */
    .text
    .align 4
    .globl tb_context_make
    .type  tb_context_make, @function
tb_context_make:
    // a0 = stackdata + stacksize (stack top)
    add     a0, a0, a1

    // 16-align the stack top
    andi    a0, a0, -16

    // reserve 112 bytes for context-data
    addi    a0, a0, -112

    // context.ra = __end (func return address)
    .option push
    .option norelax
    la      t0, __end
    .option pop
    sd      t0, 96(a0)

    // context.pc = func (entry target on first jump)
    sd      a2, 104(a0)

    // return context pointer
    ret
    .size tb_context_make, . - tb_context_make

__end:
    // exit(0)
    li      a0, 0
    call    _exit

/* jump context
 *
 * @param a0    the to-context
 * @param a1    the user private data
 *
 * @return a0   the from-context (old context)
 *         a1   the from.priv (same as input a1)
 */
    .align 4
    .globl tb_context_jump
    .type  tb_context_jump, @function
tb_context_jump:
    // reserve context space on the current stack
    addi    sp, sp, -112

    // save s0-s11
    sd      s0,   0(sp)
    sd      s1,   8(sp)
    sd      s2,  16(sp)
    sd      s3,  24(sp)
    sd      s4,  32(sp)
    sd      s5,  40(sp)
    sd      s6,  48(sp)
    sd      s7,  56(sp)
    sd      s8,  64(sp)
    sd      s9,  72(sp)
    sd      s10, 80(sp)
    sd      s11, 88(sp)

    // save ra
    sd      ra,  96(sp)

    // save pc = ra (resume target for next switch-back)
    sd      ra, 104(sp)

    // t0 = old context (sp), t1 = priv
    mv      t0, sp
    mv      t1, a1

    // switch to new context stack
    mv      sp, a0

    // restore s0-s11
    ld      s0,   0(sp)
    ld      s1,   8(sp)
    ld      s2,  16(sp)
    ld      s3,  24(sp)
    ld      s4,  32(sp)
    ld      s5,  40(sp)
    ld      s6,  48(sp)
    ld      s7,  56(sp)
    ld      s8,  64(sp)
    ld      s9,  72(sp)
    ld      s10, 80(sp)
    ld      s11, 88(sp)

    // restore ra (func return address for first entry; caller return address for resume)
    ld      ra,  96(sp)

    // load pc (first entry: func; resume: same as ra)
    ld      t2, 104(sp)

    // restore sp
    addi    sp, sp, 112

    // set from-context: a0 = old ctx, a1 = priv
    mv      a0, t0
    mv      a1, t1

    // jump to entry / resume address
    jr      t2
    .size tb_context_jump, . - tb_context_jump
