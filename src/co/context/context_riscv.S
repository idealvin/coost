/*!The Treasure Box Library
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file        context_riscv.S
 * @brief       riscv32 (RV32) context switch
 */

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
    // a0 = stackdata + stacksize
    add     a0, a0, a1

    // 16-align
    andi    a0, a0, -16

    // reserve 64 bytes for context-data
    addi    a0, a0, -64

    // context.ra = __end
    .option push
    .option norelax
    la      t0, __end
    .option pop
    sw      t0, 48(a0)

    // context.pc = func
    sw      a2, 52(a0)

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
    // reserve context space
    addi    sp, sp, -64

    // save s0-s11
    sw      s0,   0(sp)
    sw      s1,   4(sp)
    sw      s2,   8(sp)
    sw      s3,  12(sp)
    sw      s4,  16(sp)
    sw      s5,  20(sp)
    sw      s6,  24(sp)
    sw      s7,  28(sp)
    sw      s8,  32(sp)
    sw      s9,  36(sp)
    sw      s10, 40(sp)
    sw      s11, 44(sp)

    // save ra
    sw      ra,  48(sp)

    // save pc = ra
    sw      ra,  52(sp)

    // t0 = old context, t1 = priv
    mv      t0, sp
    mv      t1, a1

    // switch to new context stack
    mv      sp, a0

    // restore s0-s11
    lw      s0,   0(sp)
    lw      s1,   4(sp)
    lw      s2,   8(sp)
    lw      s3,  12(sp)
    lw      s4,  16(sp)
    lw      s5,  20(sp)
    lw      s6,  24(sp)
    lw      s7,  28(sp)
    lw      s8,  32(sp)
    lw      s9,  36(sp)
    lw      s10, 40(sp)
    lw      s11, 44(sp)

    // restore ra
    lw      ra,  48(sp)

    // load pc
    lw      t2,  52(sp)

    // restore sp
    addi    sp, sp, 64

    // set from-context
    mv      a0, t0
    mv      a1, t1

    // jump to entry / resume address
    jr      t2
    .size tb_context_jump, . - tb_context_jump
