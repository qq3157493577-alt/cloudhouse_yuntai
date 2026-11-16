/* Runtime support for the GCC build; the Keil project does not compile this file. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

extern unsigned char __HeapBase;
extern unsigned char __HeapLimit;

/* Bound newlib allocations to the heap reserved by the linker script. */
void *_sbrk(ptrdiff_t increment)
{
    static uintptr_t heap_end;
    const uintptr_t heap_base = (uintptr_t)&__HeapBase;
    const uintptr_t heap_limit = (uintptr_t)&__HeapLimit;
    uintptr_t previous;

    if (heap_end == 0)
    {
        heap_end = heap_base;
    }
    previous = heap_end;

    if (increment >= 0)
    {
        if ((uintptr_t)increment > heap_limit - heap_end)
        {
            errno = ENOMEM;
            return (void *)-1;
        }
        heap_end += (uintptr_t)increment;
    }
    else
    {
        /* Avoid negating PTRDIFF_MIN. */
        uintptr_t amount = (uintptr_t)(-(increment + 1)) + 1u;
        if (amount > heap_end - heap_base)
        {
            errno = ENOMEM;
            return (void *)-1;
        }
        heap_end -= amount;
    }
    return (void *)previous;
}

/* -nostartfiles omits crti.o; newlib's constructor runner still calls _init. */
void _init(void)
{
}

/* There is no filesystem or stdin/stdout transport. The application uses
 * Serial_Printf for UART output and snprintf/vsprintf for string formatting.
 * Return explicit errors rather than depending on semihosting or libnosys.
 */
int _close(int descriptor)
{
    (void)descriptor;
    errno = EBADF;
    return -1;
}

int _fstat(int descriptor, struct stat *status)
{
    (void)descriptor;
    (void)status;
    errno = EBADF;
    return -1;
}

int _isatty(int descriptor)
{
    (void)descriptor;
    errno = ENOTTY;
    return 0;
}

int _lseek(int descriptor, int offset, int whence)
{
    (void)descriptor;
    (void)offset;
    (void)whence;
    errno = EBADF;
    return -1;
}

int _read(int descriptor, char *buffer, int length)
{
    (void)descriptor;
    (void)buffer;
    (void)length;
    errno = EBADF;
    return -1;
}

int _write(int descriptor, const char *buffer, int length)
{
    (void)descriptor;
    (void)buffer;
    (void)length;
    errno = EBADF;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _kill(int process, int signal)
{
    (void)process;
    (void)signal;
    errno = ENOSYS;
    return -1;
}

__attribute__((noreturn)) void _exit(int status)
{
    (void)status;
    for (;;)
    {
        __asm volatile ("wfi");
    }
}
