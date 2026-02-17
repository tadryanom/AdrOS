/* AdrOS which utility — locate a command */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

static int exists_in_dir(const char* dir, const char* name) {
    int fd = open(dir, O_RDONLY);
    if (fd < 0) return 0;
    char buf[2048];
    int rc;
    while ((rc = getdents(fd, buf, sizeof(buf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(buf + off);
            if (d->d_reclen == 0) break;
            if (strcmp(d->d_name, name) == 0) {
                close(fd);
                return 1;
            }
            off += d->d_reclen;
        }
    }
    close(fd);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: which command\n");
        return 1;
    }

    static const char* path_dirs[] = { "/bin", "/sbin", NULL };
    int ret = 1;

    for (int i = 1; i < argc; i++) {
        int found = 0;
        for (int d = 0; path_dirs[d]; d++) {
            if (exists_in_dir(path_dirs[d], argv[i])) {
                printf("%s/%s\n", path_dirs[d], argv[i]);
                found = 1;
                break;
            }
        }
        if (found) ret = 0;
    }
    return ret;
}
