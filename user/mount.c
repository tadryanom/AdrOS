/* AdrOS mount utility — mount filesystems or display mounts */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syscall.h>
#include <errno.h>

static void show_mounts(void) {
    int fd = open("/proc/mounts", O_RDONLY);
    if (fd >= 0) {
        char buf[1024];
        int n;
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            write(STDOUT_FILENO, buf, (size_t)n);
        close(fd);
    } else {
        printf("tmpfs on / type overlayfs (rw)\n");
        printf("devfs on /dev type devfs (rw)\n");
        printf("procfs on /proc type procfs (ro)\n");
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        show_mounts();
        return 0;
    }

    const char* fstype = "diskfs";
    const char* device = NULL;
    const char* mountpoint = NULL;

    /* Parse options first, then collect positional args */
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            fstype = argv[++i];
        } else if (!device) {
            device = argv[i];
        } else if (!mountpoint) {
            mountpoint = argv[i];
        }
    }

    if (!device || !mountpoint) {
        fprintf(stderr, "usage: mount [-t fstype] device mountpoint\n");
        return 1;
    }

    int rc = __syscall_ret(_syscall3(SYS_MOUNT, (int)device, (int)mountpoint, (int)fstype));
    if (rc < 0) {
        fprintf(stderr, "mount: mounting %s on %s failed: %d\n", device, mountpoint, rc);
        return 1;
    }
    return 0;
}
