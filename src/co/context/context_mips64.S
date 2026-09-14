/* //////////////////////////////////////////////////////////////////////////////////////
 * implementation
 */

/* make context (refer to boost.context)
 *
 *
 *             --------------------------------------------------------------------------------------
 * stackdata: |                                                       |         context             ||
 *             --------------------------------------------------------------------------------------
 *                                                                                              (16-align)
 *
 *             ------------------------------------------------------------
 * context:   |   s0   |   s1   |   s2   |   s3   |   s4   |   s5   |
 *             ------------------------------------------------------------
 *            0        8        16       24       32       40
 *
 *                                ------------------------------------
 *                               |                                   \|/
 *                               |       __end  func             retval(from)
 *             -------------------------------------------------------------------------
 *            |   s6   |   s7   |   fp   | retval |   ra   |   pc   |   gp   |
 *             -------------------------------------------------------------------------
 *            48       56       64       72       80       88       96
 *
 *             -----------------------------------------------------
 *            |  context |  priv  | padding |      (a0-a7)        |
 *             -----------------------------------------------------
 *            104        112      120       128
 *                                          |(16-align)
 *                                          |
 *                                sp when jump to function
 *
 *
 * @param stackdata     the stack data (a0)
 * @param stacksize     the stack size (a1)
 * @param func          the entry function (a2)
 *
 * @return              the context pointer (v0)
 */
function tb_context_make, export=1

    // save the stack top to v0
    daddu $v0, $a0, $a1

    // reserve space for arguments (a0-a7) of context-function
    daddiu $v0, $v0, -64

    // 16-align of the stack top address
    move $v1, $v0
    li $v0, -16
    and $v0, $v1, $v0

    /* reserve space for context-data on context-stack
     *
     * 128 = align16(15 * 8)
     */
    daddiu $v0, $v0, -128

    // context.pc = func
    sd $a2, 88($v0)

    // context.gp = global pointer
    sd $gp, 96($v0)

    /* init retval = a writeable space (context)
     *
     * it will write retval(context, priv) when jump to a new context function entry first
     */
    daddiu $t0, $v0, 104
    sd $t0, 72($v0)

    // context.ra = address of label __end
    la $t9, __end
    sd $t9, 80($v0)

    // return pointer to context-data
    jr $ra
    nop

__end:

    // allocate stack frame space and save return address
    daddiu $sp, $sp, -32
    sd $ra, 24($sp)

    // exit(0)
    move  $a0, $zero
    la $t9, _exit
    jalr $t9
    nop

endfunc

/* jump context (refer to boost.context)
 *
 * @param retval        the from-context (a0)
 * @param context       the to-context (a1)
 * @param priv          the passed user private data (a2)
 *
 * @return              the from-context (v0: retval)
 */
function tb_context_jump, export=1

    // reserve stack space first
    daddiu $sp, $sp, -128

    // save registers and construct the current context
    sd $s0,  0($sp)
    sd $s1,  8($sp)
    sd $s2, 16($sp)
    sd $s3, 24($sp)
    sd $s4, 32($sp)
    sd $s5, 40($sp)
    sd $s6, 48($sp)
    sd $s7, 56($sp)
    sd $fp, 64($sp)
    sd $a0, 72($sp)     // save retval
    sd $ra, 80($sp)
    sd $ra, 88($sp)     // save ra as pc
    sd $gp, 96($sp)     // save gp

    // save the old context(sp) to a0
    move $a0, $sp

    // switch to the new context(sp) and stack
    move $sp, $a1

    // restore registers of the new context
    ld $s0,  0($sp)
    ld $s1,  8($sp)
    ld $s2, 16($sp)
    ld $s3, 24($sp)
    ld $s4, 32($sp)
    ld $s5, 40($sp)
    ld $s6, 48($sp)
    ld $s7, 56($sp)
    ld $fp, 64($sp)
    ld $t0, 72($sp)     // load retval
    ld $ra, 80($sp)
    ld $t9, 88($sp)     // load t9 = pc
    ld $gp, 96($sp)     // load gp

    // restore stack space
    daddiu $sp, $sp, 128

    // return from-context(context: a0, priv: a1) from jump
    sd $a0, ($t0)
    sd $a2, 8($t0)

    // pass old-context(context: a0, priv: a1) arguments to the context function
    move $a1, $a2

    /* jump to the return or entry address(pc)
     *
     *             ----------------------
     * context:   |   args   |  padding  |
     *             ----------------------
     *            0
     *            |
     *            sp
     */
    jr $t9
    nop

endfunc
