struct wasi_iov {
        uint32_t iov_base;
        uint32_t iov_len;
};

#define WASI_FILETYPE_UNKNOWN 0
#define WASI_FILETYPE_BLOCK_DEVICE 1
#define WASI_FILETYPE_CHARACTER_DEVICE 2
#define WASI_FILETYPE_DIRECTORY 3
#define WASI_FILETYPE_REGULAR_FILE 4
#define WASI_FILETYPE_SOCKET_DGRAM 5
#define WASI_FILETYPE_SOCKET_STREAM 6
#define WASI_FILETYPE_SYMBOLIC_LINK 7

#define WASI_FDFLAG_APPEND 1
#define WASI_FDFLAG_DSYNC 2
#define WASI_FDFLAG_NONBLOCK 4
#define WASI_FDFLAG_RSYNC 8
#define WASI_FDFLAG_SYNC 16

#define WASI_OFLAG_CREAT 1
#define WASI_OFLAG_DIRECTORY 2
#define WASI_OFLAG_EXCL 4
#define WASI_OFLAG_TRUNC 8

#define WASI_RIGHT_FD_READ 2
#define WASI_RIGHT_FD_WRITE 4

struct wasi_fdstat {
        uint8_t fs_filetype;
        uint8_t pad1;
        uint16_t fs_flags;
        uint8_t pad2[4];
        uint64_t fs_rights_base;
        uint64_t fs_rights_inheriting;
};
_Static_assert(sizeof(struct wasi_fdstat) == 24, "wasi_fdstat");

struct wasi_fd_prestat {
        uint8_t type;
        uint8_t pad[3];
        uint32_t dir_name_len;
};
_Static_assert(sizeof(struct wasi_fd_prestat) == 8, "wasi_fd_prestat");

#define WASI_PREOPEN_TYPE_DIR 0

struct wasi_filestat {
        uint64_t dev;
        uint64_t ino;
        uint8_t type;
        uint8_t pad[7];
        uint64_t linkcount;
        uint64_t size;
        uint64_t atim;
        uint64_t mtim;
        uint64_t ctim;
};
_Static_assert(sizeof(struct wasi_filestat) == 64, "wasi_filestat");
