#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char end;
extern char _estack;
static char *heap_end;

void *_sbrk(ptrdiff_t increment)
{
    char *previous;
    if (heap_end == 0) heap_end = &end;
    previous = heap_end;
    if ((uintptr_t)(heap_end + increment) > (uintptr_t)&_estack - 1024U) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_end += increment;
    return previous;
}

int _write(int file, char *data, int length)
{
    (void)file;
    (void)data;
    return length;
}

int _read(int file, char *data, int length)
{
    (void)file; (void)data; (void)length;
    errno = EIO;
    return -1;
}

int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *status) { (void)file; status->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
off_t _lseek(int file, off_t offset, int direction)
{ (void)file; (void)offset; (void)direction; return 0; }
int _getpid(void) { return 1; }
int _kill(int pid, int signal) { (void)pid; (void)signal; errno = EINVAL; return -1; }
void _exit(int status) { (void)status; while (1) { } }
