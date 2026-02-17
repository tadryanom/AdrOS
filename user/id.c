/* AdrOS id utility — display user and group IDs */
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("uid=%d gid=%d euid=%d egid=%d\n",
           getuid(), getgid(), geteuid(), getegid());
    return 0;
}
