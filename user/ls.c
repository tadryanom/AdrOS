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

#define LS_MAX_ENTRIES 512

struct ls_entry {
    char name[256];
    unsigned char type;
};

static struct ls_entry entries[LS_MAX_ENTRIES];

static int cmp_entry(const void* a, const void* b) {
    return strcmp(((const struct ls_entry*)a)->name,
                  ((const struct ls_entry*)b)->name);
}

static void ls_dir(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ls: cannot access '%s': No such file or directory\n", path);
        return;
    }

    int count = 0;
    char buf[2048];
    int rc;
    while ((rc = getdents(fd, buf, sizeof(buf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(buf + off);
            if (d->d_reclen == 0) break;

            if (!aflag && d->d_name[0] == '.') {
                off += d->d_reclen;
                continue;
            }

            if (count < LS_MAX_ENTRIES) {
                strncpy(entries[count].name, d->d_name, 255);
                entries[count].name[255] = '\0';
                entries[count].type = d->d_type;
                count++;
            }
            off += d->d_reclen;
        }
    }
    close(fd);

    qsort(entries, count, sizeof(struct ls_entry), cmp_entry);

    for (int i = 0; i < count; i++) {
        if (lflag) {
            char fullpath[512];
            size_t plen = strlen(path);
            if (plen > 0 && path[plen - 1] == '/')
                snprintf(fullpath, sizeof(fullpath), "%s%s", path, entries[i].name);
            else
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entries[i].name);

            struct stat st;
            int have_stat = (stat(fullpath, &st) == 0);

            char type = '-';
            if (entries[i].type == DT_DIR) type = 'd';
            else if (entries[i].type == DT_CHR) type = 'c';
            else if (entries[i].type == DT_LNK) type = 'l';
            else if (entries[i].type == DT_BLK) type = 'b';

            char perms[10];
            if (have_stat) {
                unsigned m = (unsigned)st.st_mode;
                perms[0] = (m & S_IRUSR) ? 'r' : '-';
                perms[1] = (m & S_IWUSR) ? 'w' : '-';
                perms[2] = (m & S_IXUSR) ? 'x' : '-';
                perms[3] = (m & S_IRGRP) ? 'r' : '-';
                perms[4] = (m & S_IWGRP) ? 'w' : '-';
                perms[5] = (m & S_IXGRP) ? 'x' : '-';
                perms[6] = (m & S_IROTH) ? 'r' : '-';
                perms[7] = (m & S_IWOTH) ? 'w' : '-';
                perms[8] = (m & S_IXOTH) ? 'x' : '-';
                perms[9] = '\0';
            } else {
                strcpy(perms, "---------");
            }

            unsigned long sz = have_stat ? (unsigned long)st.st_size : 0;
            unsigned nlink = have_stat ? (unsigned)st.st_nlink : 1;

            printf("%c%s %2u root root %8lu %s\n",
                   type, perms, nlink, sz, entries[i].name);
        } else {
            printf("%s\n", entries[i].name);
        }
    }
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
