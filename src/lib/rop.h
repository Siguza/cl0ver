#ifndef ROP_H
#define ROP_H

#include <mach/mach_types.h>    // task_t

#include "common.h"             // file_t

void* get_stack_pivot(file_t *kernel);

void rop_get_kernel_task(file_t *kernel, void ***chain, task_t *task);

#endif
