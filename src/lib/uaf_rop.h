#ifndef UAF_ROP_H
#define UAF_ROP_H

#include "common.h"             // file_t
#include "io.h"                 // OSString

void uaf_parse(const OSString *fake);

void** uaf_rop_stack(void);

void uaf_rop(file_t *kernel);

#endif
