#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

__attribute__((weak)) intmax_t strtoimax(const char *__restrict nptr, char **__restrict endptr, int base)
{
    return (intmax_t)strtoll(nptr, endptr, base);
}

__attribute__((weak)) uintmax_t strtoumax(const char *__restrict nptr, char **__restrict endptr, int base)
{
    return (uintmax_t)strtoull(nptr, endptr, base);
}

__attribute__((weak)) uid_t getuid(void)
{
    return 0;
}

__attribute__((weak)) gid_t getgid(void)
{
    return 0;
}

__attribute__((weak)) uid_t geteuid(void)
{
    return 0;
}

__attribute__((weak)) gid_t getegid(void)
{
    return 0;
}

__attribute__((weak)) int fchmod(int fd, mode_t mode)
{
    (void)fd;
    (void)mode;
    return 0;
}

/* PSL1GHT is single-threaded; provide pthread mutex stubs when libpthread is not linked */
__attribute__((weak)) int pthread_mutex_init(void *mutex, const void *attr)
{
    (void)mutex;
    (void)attr;
    return 0;
}

__attribute__((weak)) int pthread_mutex_destroy(void *mutex)
{
    (void)mutex;
    return 0;
}

__attribute__((weak)) int pthread_mutex_lock(void *mutex)
{
    (void)mutex;
    return 0;
}

__attribute__((weak)) int pthread_mutex_unlock(void *mutex)
{
    (void)mutex;
    return 0;
}

__attribute__((weak)) int pthread_mutex_trylock(void *mutex)
{
    (void)mutex;
    return 0;
}
