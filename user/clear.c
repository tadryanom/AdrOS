/* AdrOS clear utility — clear the terminal screen */
#include <unistd.h>

int main(void) {
    /* ANSI escape: clear screen + move cursor to top-left */
    write(STDOUT_FILENO, "\033[2J\033[H", 7);
    return 0;
}
