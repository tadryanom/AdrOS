#ifndef KCONSOLE_H
#define KCONSOLE_H

// Enter the kernel emergency console (kernel-mode, like HelenOS kconsole).
// Called when VFS mount or init fails. Runs in a loop until reboot.
void kconsole_enter(void);

#endif
