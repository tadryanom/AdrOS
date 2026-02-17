/* AdrOS tee utility — read stdin, write to stdout and files */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
    int aflag = 0;
    int fds[16];
    int nfds = 0;

    for (int i = 1; i < argc && nfds < 16; i++) {
        if (strcmp(argv[i], "-a") == 0) { aflag = 1; continue; }
        int flags = O_WRONLY | O_CREAT;
        flags |= aflag ? O_APPEND : O_TRUNC;
        int fd = open(argv[i], flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "tee: %s: cannot open\n", argv[i]);
            continue;
        }
        fds[nfds++] = fd;
    }

    char buf[4096];
    int n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, (size_t)n);
        for (int i = 0; i < nfds; i++)
            write(fds[i], buf, (size_t)n);
    }

    for (int i = 0; i < nfds; i++) close(fds[i]);
    return 0;
}
