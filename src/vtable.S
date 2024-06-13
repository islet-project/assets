/*
 * Copyright (c) 2023, Arm Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

.macro current_exception_spx elx:req handler:req
    save_volatile_to_stack \elx
    bl \handler
    restore_from_stack_and_return \elx
.endm

.macro save_volatile_to_stack elx:req
    /* Reserve stack space and save registers x0-x18, x29 & x30. */
    stp x0, x1, [sp, #-(8 * 24)]!
    stp x2, x3, [sp, #8 * 2]
    stp x4, x5, [sp, #8 * 4]
    stp x6, x7, [sp, #8 * 6]
    stp x8, x9, [sp, #8 * 8]
    stp x10, x11, [sp, #8 * 10]
    stp x12, x13, [sp, #8 * 12]
    stp x14, x15, [sp, #8 * 14]
    stp x16, x17, [sp, #8 * 16]
    str x18, [sp, #8 * 18]
    stp x29, x30, [sp, #8 * 20]

    /*
     * Save elr_elx & spsr_elx. This such that we can take nested exception
     * and still be able to unwind.
     */
    mrs x0, elr_\elx
    mrs x1, spsr_\elx
    stp x0, x1, [sp, #8 * 22]
.endm

.macro restore_from_stack_and_return elx:req
    /* Restore registers x2-x18, x29 & x30. */
    ldp x2, x3, [sp, #8 * 2]
    ldp x4, x5, [sp, #8 * 4]
    ldp x6, x7, [sp, #8 * 6]
    ldp x8, x9, [sp, #8 * 8]
    ldp x10, x11, [sp, #8 * 10]
    ldp x12, x13, [sp, #8 * 12]
    ldp x14, x15, [sp, #8 * 14]
    ldp x16, x17, [sp, #8 * 16]
    ldr x18, [sp, #8 * 18]
    ldp x29, x30, [sp, #8 * 20]

    cbnz x0, 1f

    /* Restore register elr_elx using x1 as scratch. */
    ldr x1, [sp, #8 * 22]
    msr elr_\elx, x1

1:
    /* Restore register spsr_elx using x1 as scratch. */
    ldr x1, [sp, #8 * 23]
    msr spsr_\elx, x1

    /* Restore x0 & x1, and release stack space. */
    ldp x0, x1, [sp], #8 * 24

    eret
.endm

  .section .text.vtable, "ax"
  .align 12

.global vector_table
vector_table:

// ------------------------------------------------------------
// Current EL with SP0
// ------------------------------------------------------------
  .balign 128
sync_current_el_sp0:
  B        .                    //        Synchronous

  .balign 128
irq_current_el_sp0:
  B        .                    //        IRQ

  .balign 128
fiq_current_el_sp0:
  B        .                    //        FIQ

  .balign 128
serror_current_el_sp0:
  B        .                    //        SError

// ------------------------------------------------------------
// Current EL with SPx
// ------------------------------------------------------------

  .balign 128
sync_current_el_spx:
  current_exception_spx el1 sync_exception_current

  .balign 128
irq_current_el_spx:
  current_exception_spx el1 irq_current

  .balign 128
fiq_current_el_spx:
  current_exception_spx el1 irq_current

  .balign 128
serror_current_el_spx:
  B        .                    //        SError

// ------------------------------------------------------------
// Lower EL using AArch64
// ------------------------------------------------------------

  .balign 128
sync_lower_el_aarch64:
   B        .

  .balign 128
irq_lower_el_aarch64:
  B        .                    //        IRQ

  .balign 128
fiq_lower_el_aarch64:
  B        .                    //        FIQ

  .balign 128
serror_lower_el_aarch64:
  B        .                    //        SError

// ------------------------------------------------------------
// Lower EL using AArch32
// ------------------------------------------------------------

  .balign 128
sync_lower_el_aarch32:
   B        .

  .balign 128
irq_lower_el_aarch32:
  B        .                    //        IRQ

  .balign 128
fiq_lower_el_aarch32:
  B        .                    //        FIQ

  .balign 128
serror_lower_el_aarch32:
  B        .                    //        SError

