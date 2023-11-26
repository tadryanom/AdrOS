#include <screen.h>
#include <system.h>

// Some screen stuff.
#define COLUMNS   80
#define LINES     25
#define BACKCOLOR 1       // blue
#define FORECOLOR 15      // white
#define VIDEO     0xB8000 // The video memory address.

static volatile u16int *video;
static s32int xpos;
static s32int ypos;

// Scrolls the screen
static void scroll (void)
{
    if (ypos >= LINES)
    {
        /* 
         * Move the current text chunk that makes up the screen
         * back in the buffer by a line 
         */
        s32int i;
        for (i = 0; i < (LINES - 1) * COLUMNS; i++)
            *(video + i) = *(video + i + COLUMNS);

        /* 
         * Finally, we set the chunk of memory that occupies
         * the last line of text to our 'blank' character 
         */
        for (i = (LINES - 1) * COLUMNS; i < LINES * COLUMNS; i++)
            *(video + i) = 0x20 | (((BACKCOLOR << 4) | (FORECOLOR & 0x0F)) << 8);

        ypos = LINES - 1;
    }
}

// Updates the hardware cursor.
static void move_cursor (void)
{
    u16int cpos = ypos * COLUMNS + xpos;
    outportb (0x3D4, 0xE);         // Tell the VGA board we are setting the high cursor byte.
    outportb (0x3D5, cpos >> 0x8); // Send the high cursor byte.
    outportb (0x3D4, 0xF);         // Tell the VGA board we are setting the low cursor byte.
    outportb (0x3D5, cpos);        // Send the low cursor byte.
}

// Sets our text-mode VGA pointer, then clears the screen for us
void init_video (void)
{
    video = (u16int *) VIDEO;
    cls();
}

// Clear the screen and initialize VIDEO, XPOS and YPOS.
void cls (void)
{
    for (s32int i = 0; i < COLUMNS * LINES; i++)
        *(video + i) = 0x20 | (((BACKCOLOR << 4) | (FORECOLOR & 0x0F)) << 8);

    xpos = 0;
    ypos = 0;
    move_cursor();
}

// Put the character C on the screen.
void put_char (s8int c)
{
    if (c == 0x08 && xpos) { // Back-space
        xpos--;
    } else if (c == 0x09) { // Horizontal Tabulation
        xpos = (xpos + 8) & ~(8 - 1);
    } else if (c == 0x0A) { // Line Feed
        xpos = 0;
        ypos++;
    } else if (c == 0x0D) { // Carriage Return
        xpos = 0;
    } else if ((c >= 0x20) && (c <= 0x7E)) {
        *(video + (xpos + ypos * COLUMNS)) = c | (((BACKCOLOR << 4) | (FORECOLOR & 0x0F)) << 8);
        xpos++;
    }
    if (xpos >= COLUMNS) {
        xpos = 0;
        ypos++;
    }
    scroll();
    move_cursor();
}
