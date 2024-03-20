#include "wasi_littlefs_config.h"

#if defined(TOYWASM_LITTLEFS_USE_PREFIX)

#define LFS_PREFIX TOYWASM_LITTLEFS_PREFIX
#define LFS_PREFIX_UPPER TOYWASM_LITTLEFS_PREFIX_UPPER

#define _CAT(a, b) __CAT(a, b)
#define __CAT(a, b) a##b

#define _STR(a) __STR(a)
#define __STR(a) #a

#define HDR _STR(LFS_PREFIX.h)
#include HDR

#define LFS_ERR_OK _CAT(LFS_PREFIX_UPPER, _ERR_OK)
#define LFS_ERR_IO _CAT(LFS_PREFIX_UPPER, _ERR_IO)
#define LFS_ERR_CORRUPT _CAT(LFS_PREFIX_UPPER, _ERR_CORRUPT)
#define LFS_ERR_NOENT _CAT(LFS_PREFIX_UPPER, _ERR_NOENT)
#define LFS_ERR_EXIST _CAT(LFS_PREFIX_UPPER, _ERR_EXIST)
#define LFS_ERR_NOTDIR _CAT(LFS_PREFIX_UPPER, _ERR_NOTDIR)
#define LFS_ERR_ISDIR _CAT(LFS_PREFIX_UPPER, _ERR_ISDIR)
#define LFS_ERR_NOTEMPTY _CAT(LFS_PREFIX_UPPER, _ERR_NOTEMPTY)
#define LFS_ERR_BADF _CAT(LFS_PREFIX_UPPER, _ERR_BADF)
#define LFS_ERR_FBIG _CAT(LFS_PREFIX_UPPER, _ERR_FBIG)
#define LFS_ERR_INVAL _CAT(LFS_PREFIX_UPPER, _ERR_INVAL)
#define LFS_ERR_NOSPC _CAT(LFS_PREFIX_UPPER, _ERR_NOSPC)
#define LFS_ERR_NOMEM _CAT(LFS_PREFIX_UPPER, _ERR_NOMEM)
#define LFS_ERR_NOATTR _CAT(LFS_PREFIX_UPPER, _ERR_NOATTR)
#define LFS_ERR_NAMETOOLONG _CAT(LFS_PREFIX_UPPER, _ERR_NAMETOOLONG)

#define LFS_O_CREAT _CAT(LFS_PREFIX_UPPER, _O_CREAT)
#define LFS_O_TRUNC _CAT(LFS_PREFIX_UPPER, _O_TRUNC)
#define LFS_O_EXCL _CAT(LFS_PREFIX_UPPER, _O_EXCL)
#define LFS_O_APPEND _CAT(LFS_PREFIX_UPPER, _O_APPEND)
#define LFS_O_RDONLY _CAT(LFS_PREFIX_UPPER, _O_RDONLY)
#define LFS_O_WRONLY _CAT(LFS_PREFIX_UPPER, _O_WRONLY)
#define LFS_O_RDWR _CAT(LFS_PREFIX_UPPER, _O_RDWR)

#define LFS_TYPE_REG _CAT(LFS_PREFIX_UPPER, _TYPE_REG)
#define LFS_TYPE_DIR _CAT(LFS_PREFIX_UPPER, _TYPE_DIR)

#define LFS_SEEK_SET _CAT(LFS_PREFIX_UPPER, _SEEK_SET)
#define LFS_SEEK_CUR _CAT(LFS_PREFIX_UPPER, _SEEK_CUR)
#define LFS_SEEK_END _CAT(LFS_PREFIX_UPPER, _SEEK_END)

#define lfs_config _CAT(LFS_PREFIX, _config)
#define lfs_size_t _CAT(LFS_PREFIX, _size_t)
#define lfs_ssize_t _CAT(LFS_PREFIX, _ssize_t)
#define lfs_soff_t _CAT(LFS_PREFIX, _soff_t)
#define lfs_off_t _CAT(LFS_PREFIX, _off_t)
#define lfs_block_t _CAT(LFS_PREFIX, _block_t)
#define lfs_file_t _CAT(LFS_PREFIX, _file_t)
#define lfs_dir_t _CAT(LFS_PREFIX, _dir_t)
#define lfs_t _CAT(LFS_PREFIX, _t)
#define lfs_error _CAT(LFS_PREFIX, _error)
#define lfs_info _CAT(LFS_PREFIX, _info)

#define lfs_mount _CAT(LFS_PREFIX, _mount)
#define lfs_unmount _CAT(LFS_PREFIX, _unmount)
#define lfs_file_truncate _CAT(LFS_PREFIX, _file_truncate)
#define lfs_file_seek _CAT(LFS_PREFIX, _file_seek)
#define lfs_file_sync _CAT(LFS_PREFIX, _file_sync)
#define lfs_file_size _CAT(LFS_PREFIX, _file_size)
#define lfs_file_open _CAT(LFS_PREFIX, _file_open)
#define lfs_file_read _CAT(LFS_PREFIX, _file_read)
#define lfs_file_write _CAT(LFS_PREFIX, _file_write)
#define lfs_file_close _CAT(LFS_PREFIX, _file_close)
#define lfs_dir_read _CAT(LFS_PREFIX, _dir_read)
#define lfs_dir_rewind _CAT(LFS_PREFIX, _dir_rewind)
#define lfs_dir_seek _CAT(LFS_PREFIX, _dir_seek)
#define lfs_dir_tell _CAT(LFS_PREFIX, _dir_tell)
#define lfs_dir_open _CAT(LFS_PREFIX, _dir_open)
#define lfs_dir_close _CAT(LFS_PREFIX, _dir_close)
#define lfs_stat _CAT(LFS_PREFIX, _stat)
#define lfs_rename _CAT(LFS_PREFIX, _rename)
#define lfs_remove _CAT(LFS_PREFIX, _remove)
#define lfs_mkdir _CAT(LFS_PREFIX, _mkdir)

#else /* defined(TOYWASM_LITTLEFS_USE_PREFIX) */

#include "lfs.h"

#endif /* defined(TOYWASM_LITTLEFS_USE_PREFIX) */
