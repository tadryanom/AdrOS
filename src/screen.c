#include <screen.h>

/* Some screen stuff. */
#define COLUMNS   80
#define LINES     24
#define ATTRIBUTE 7
#define VIDEO     0xB8000 /* The video memory address. */

#define RED   0xFF0000
#define GREEN 0xFF00
#define BLUE  0xFF

static s32int xpos;
static s32int ypos;
static volatile u8int *video;

/* Clear the screen and initialize VIDEO, XPOS and YPOS. */
void cls (void)
{
    s32int i;
    video = (u8int *) VIDEO;
  
    for (i = 0; i < COLUMNS * LINES * 2; i++)
        *(video + i) = 0;

    xpos = 0;
    ypos = 0;
}

/* Put the character C on the screen. */
void put_char(s8int c)
{
    if (c == '\n' || c == '\r') {
        newline:
        xpos = 0;
        ypos++;
        if (ypos >= LINES)
            ypos = 0;
        return;
    }

    *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
    *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;

    xpos++;
    if (xpos >= COLUMNS)
        goto newline;
}
