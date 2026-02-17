/* AdrOS ls utility */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

static int aflag = 0;   /* -a: show hidden files */
static int lflag = 0;   /* -l: long format */

static void ls_dir(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ls: cannot access '%s': No such file or directory\n", path);
        return;
    }

    char buf[2048];
    int rc;
    while ((rc = getdents(fd, buf, sizeof(buf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(buf + off);
            if (d->d_reclen == 0) break;

            /* skip hidden files unless -a */
            if (!aflag && d->d_name[0] == '.') {
                off += d->d_reclen;
                continue;
            }

            if (lflag) {
                char type = '-';
                if (d->d_type == DT_DIR) type = 'd';
                else if (d->d_type == DT_CHR) type = 'c';
                else if (d->d_type == DT_LNK) type = 'l';
                else if (d->d_type == DT_BLK) type = 'b';
                printf("%c  %s\n", type, d->d_name);
            } else {
                printf("%s\n", d->d_name);
            }
            off += d->d_reclen;
        }
    }

    close(fd);
}

int main(int argc, char** argv) {
    int npath = 0;
    const char* paths[64];

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            const char* f = argv[i] + 1;
            while (*f) {
                if (*f == 'a') aflag = 1;
                else if (*f == 'l') lflag = 1;
                else {
                    fprintf(stderr, "ls: invalid option -- '%c'\n", *f);
                    return 1;
                }
                f++;
            }
        } else {
            if (npath < 64) paths[npath++] = argv[i];
        }
    }

    if (npath == 0) {
        ls_dir(".");
    } else {
        for (int i = 0; i < npath; i++) {
            if (npath > 1) printf("%s:\n", paths[i]);
            ls_dir(paths[i]);
            if (npath > 1 && i < npath - 1) printf("\n");
        }
    }

    return 0;
}
