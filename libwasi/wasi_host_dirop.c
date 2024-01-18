#define _DARWIN_C_SOURCE /* symlink etc */

#include <sys/stat.h>
#include <sys/time.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "wasi_dirop.h"

int
wasi_host_open(const struct path_info *pi, int oflags, unsigned int mode)
{
        return open(pi->hostpath, oflags, mode);
}

int
wasi_host_unlink(const struct path_info *pi)
{
        return unlink(pi->hostpath);
}

int
wasi_host_mkdir(const struct path_info *pi)
{
        return mkdir(pi->hostpath, 0777);
}

int
wasi_host_rmdir(const struct path_info *pi)
{
        return rmdir(pi->hostpath);
}

int
wasi_host_symlink(const char *target_buf, const struct path_info *pi)
{
        return symlink(target_buf, pi->hostpath);
}

int
wasi_host_readlink(const struct path_info *pi, char *buf, size_t buflen)
{
        return readlink(pi->hostpath, buf, buflen);
}

int
wasi_host_link(const struct path_info *pi1, const struct path_info *pi2)
{
        return link(pi1->hostpath, pi2->hostpath);
}

int
wasi_host_rename(const struct path_info *pi1, const struct path_info *pi2)
{
        return rename(pi1->hostpath, pi2->hostpath);
}

int
wasi_host_stat(const struct path_info *pi, struct stat *stp)
{
        return stat(pi->hostpath, stp);
}

int
wasi_host_lstat(const struct path_info *pi, struct stat *stp)
{
        return lstat(pi->hostpath, stp);
}

int
wasi_host_utimes(const struct path_info *pi, const struct timeval *tvp)
{
        return utimes(pi->hostpath, tvp);
}

int
wasi_host_lutimes(const struct path_info *pi, const struct timeval *tvp)
{
        return lutimes(pi->hostpath, tvp);
}
