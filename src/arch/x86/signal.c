#include "arch_signal.h"
#include "arch/x86/signal.h"
#include "process.h"
#include "uaccess.h"
#include "errno.h"

#if defined(__i386__)

int arch_sigreturn(void* opaque, const void* user_frame)
{
    struct registers* regs = (struct registers*)opaque;
    if (!regs) return -EINVAL;
    if (!current_process) return -EINVAL;
    if ((regs->cs & 3U) != 3U) return -EPERM;
    if (!user_frame) return -EFAULT;

    if (user_range_ok(user_frame, sizeof(struct sigframe)) == 0)
        return -EFAULT;

    struct sigframe f;
    if (copy_from_user(&f, user_frame, sizeof(f)) < 0)
        return -EFAULT;
    if (f.magic != SIGFRAME_MAGIC)
        return -EINVAL;

    if ((f.saved.cs & 3U) != 3U) return -EPERM;
    if ((f.saved.ss & 3U) != 3U) return -EPERM;

    // Sanitize eflags: clear IOPL (bits 12-13) to prevent privilege escalation,
    // ensure IF (bit 9) is set so interrupts remain enabled in usermode.
    f.saved.eflags = (f.saved.eflags & ~0x3000U) | 0x200U;

    // Restore the full saved trapframe. The interrupt stub will pop these regs and iret.
    *regs = f.saved;
    return 0;
}

#endif /* __i386__ */
