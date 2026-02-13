#ifndef ULIBC_STDIO_H
#define ULIBC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define BUFSIZ      256
#define EOF         (-1)
#define FOPEN_MAX   16

#define _STDIO_READ  0x01
#define _STDIO_WRITE 0x02
#define _STDIO_EOF   0x04
#define _STDIO_ERR   0x08
#define _STDIO_LBUF  0x10  /* line-buffered (flush on \n) */
#define _STDIO_UNBUF 0x20  /* unbuffered (flush every write) */

typedef struct _FILE {
    int     fd;
    int     flags;
    char    buf[BUFSIZ];
    int     buf_pos;
    int     buf_len;
} FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

FILE*   fopen(const char* path, const char* mode);
int     fclose(FILE* fp);
size_t  fread(void* ptr, size_t size, size_t nmemb, FILE* fp);
size_t  fwrite(const void* ptr, size_t size, size_t nmemb, FILE* fp);
int     fseek(FILE* fp, long offset, int whence);
long    ftell(FILE* fp);
void    rewind(FILE* fp);
int     fflush(FILE* fp);
int     fgetc(FILE* fp);
char*   fgets(char* s, int size, FILE* fp);
int     fputc(int c, FILE* fp);
int     fputs(const char* s, FILE* fp);
int     feof(FILE* fp);
int     ferror(FILE* fp);
int     fprintf(FILE* fp, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
int     vfprintf(FILE* fp, const char* fmt, va_list ap);

int     putchar(int c);
int     puts(const char* s);
int     printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
int     vprintf(const char* fmt, va_list ap);
int     sprintf(char* buf, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
int     snprintf(char* buf, size_t size, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
int     vsnprintf(char* buf, size_t size, const char* fmt, va_list ap);
int     sscanf(const char* str, const char* fmt, ...);
int     remove(const char* path);
int     rename(const char* oldpath, const char* newpath);

/* Buffering modes for setvbuf (POSIX values) */
#define _IOFBF 0  /* fully buffered */
#define _IOLBF 1  /* line buffered */
#define _IONBF 2  /* unbuffered */
int     setvbuf(FILE* fp, char* buf, int mode, size_t size);
void    setbuf(FILE* fp, char* buf);

#endif
