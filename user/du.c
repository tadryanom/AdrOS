/* AdrOS du utility — estimate file space usage */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

static int sflag = 0; /* -s: summary only */

static long du_path(const char* path, int print) {
    struct stat st;
    if (stat(path, &st) < 0) {
        fprintf(stderr, "du: cannot access '%s'\n", path);
        return 0;
    }

    if (!(st.st_mode & 0040000)) {
        long blocks = (st.st_size + 511) / 512;
        if (print && !sflag) printf("%ld\t%s\n", blocks, path);
        return blocks;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    long total = 0;
    char buf[2048];
    int rc;
    while ((rc = getdents(fd, buf, sizeof(buf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(buf + off);
            if (d->d_reclen == 0) break;
            if (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0) {
                char child[512];
                if (path[strlen(path)-1] == '/')
                    snprintf(child, sizeof(child), "%s%s", path, d->d_name);
                else
                    snprintf(child, sizeof(child), "%s/%s", path, d->d_name);
                total += du_path(child, print);
            }
            off += d->d_reclen;
        }
    }
    close(fd);

    if (print && !sflag) printf("%ld\t%s\n", total, path);
    return total;
}

int main(int argc, char** argv) {
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        const char* f = argv[argi] + 1;
        while (*f) {
            if (*f == 's') sflag = 1;
            f++;
        }
        argi++;
    }

    if (argi >= argc) {
        long total = du_path(".", 1);
        if (sflag) printf("%ld\t.\n", total);
    } else {
        for (int i = argi; i < argc; i++) {
            long total = du_path(argv[i], 1);
            if (sflag) printf("%ld\t%s\n", total, argv[i]);
        }
    }
    return 0;
}
