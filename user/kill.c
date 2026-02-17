/* AdrOS kill utility — send signal to process */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "usage: kill [-SIGNAL] PID...\n");
        return 1;
    }

    int sig = 15; /* SIGTERM */
    int start = 1;

    if (argv[1][0] == '-') {
        const char* s = argv[1] + 1;
        if (strcmp(s, "9") == 0 || strcmp(s, "KILL") == 0) sig = 9;
        else if (strcmp(s, "15") == 0 || strcmp(s, "TERM") == 0) sig = 15;
        else if (strcmp(s, "2") == 0 || strcmp(s, "INT") == 0) sig = 2;
        else if (strcmp(s, "1") == 0 || strcmp(s, "HUP") == 0) sig = 1;
        else if (strcmp(s, "0") == 0) sig = 0;
        else sig = atoi(s);
        start = 2;
    }

    int rc = 0;
    for (int i = start; i < argc; i++) {
        int pid = atoi(argv[i]);
        if (pid <= 0) {
            fprintf(stderr, "kill: invalid pid '%s'\n", argv[i]);
            rc = 1;
            continue;
        }
        if (kill(pid, sig) < 0) {
            fprintf(stderr, "kill: %d: no such process\n", pid);
            rc = 1;
        }
    }
    return rc;
}
