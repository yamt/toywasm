
#define WASI_U16_ALIGN 2
#define WASI_U32_ALIGN 4
#define WASI_U64_ALIGN 8

struct wasi_iov {
        uint32_t iov_base;
        uint32_t iov_len;
};
#define WASI_IOV_ALIGN WASI_U32_ALIGN

#define WASI_FILETYPE_UNKNOWN 0
#define WASI_FILETYPE_BLOCK_DEVICE 1
#define WASI_FILETYPE_CHARACTER_DEVICE 2
#define WASI_FILETYPE_DIRECTORY 3
#define WASI_FILETYPE_REGULAR_FILE 4
#define WASI_FILETYPE_SOCKET_DGRAM 5
#define WASI_FILETYPE_SOCKET_STREAM 6
#define WASI_FILETYPE_SYMBOLIC_LINK 7

#define WASI_LOOKUPFLAG_SYMLINK_FOLLOW 1

#define WASI_FSTFLAG_ATIM 1
#define WASI_FSTFLAG_ATIM_NOW 2
#define WASI_FSTFLAG_MTIM 4
#define WASI_FSTFLAG_MTIM_NOW 8

#define WASI_FDFLAG_APPEND 1
#define WASI_FDFLAG_DSYNC 2
#define WASI_FDFLAG_NONBLOCK 4
#define WASI_FDFLAG_RSYNC 8
#define WASI_FDFLAG_SYNC 16

#define WASI_OFLAG_CREAT 1
#define WASI_OFLAG_DIRECTORY 2
#define WASI_OFLAG_EXCL 4
#define WASI_OFLAG_TRUNC 8

#define WASI_RIGHT_FD_DATASYNC (UINT64_C(1) << 0)
#define WASI_RIGHT_FD_READ (UINT64_C(1) << 1)
#define WASI_RIGHT_FD_SEEK (UINT64_C(1) << 2)
#define WASI_RIGHT_FD_FDSTAT_SET_FLAGS (UINT64_C(1) << 3)
#define WASI_RIGHT_FD_SYNC (UINT64_C(1) << 4)
#define WASI_RIGHT_FD_TELL (UINT64_C(1) << 5)
#define WASI_RIGHT_FD_WRITE (UINT64_C(1) << 6)
#define WASI_RIGHT_FD_ADVISE (UINT64_C(1) << 7)
#define WASI_RIGHT_FD_ALLOCATE (UINT64_C(1) << 8)
#define WASI_RIGHT_PATH_CREATE_DIRECTORY (UINT64_C(1) << 9)
#define WASI_RIGHT_PATH_CREATE_FILE (UINT64_C(1) << 10)
#define WASI_RIGHT_PATH_LINK_SOURCE (UINT64_C(1) << 11)
#define WASI_RIGHT_PATH_LINK_TARGET (UINT64_C(1) << 12)
#define WASI_RIGHT_PATH_OPEN (UINT64_C(1) << 13)
#define WASI_RIGHT_FD_READDIR (UINT64_C(1) << 14)
#define WASI_RIGHT_PATH_READLINK (UINT64_C(1) << 15)
#define WASI_RIGHT_PATH_RENAME_SOURCE (UINT64_C(1) << 16)
#define WASI_RIGHT_PATH_RENAME_TARGET (UINT64_C(1) << 17)
#define WASI_RIGHT_PATH_FILESTAT_GET (UINT64_C(1) << 18)
#define WASI_RIGHT_PATH_FILESTAT_SET_SIZE (UINT64_C(1) << 19)
#define WASI_RIGHT_PATH_FILESTAT_SET_TIMES (UINT64_C(1) << 20)
#define WASI_RIGHT_FD_FILESTAT_GET (UINT64_C(1) << 21)
#define WASI_RIGHT_FD_FILESTAT_SET_SIZE (UINT64_C(1) << 22)
#define WASI_RIGHT_FD_FILESTAT_SET_TIMES (UINT64_C(1) << 23)
#define WASI_RIGHT_PATH_SYMLINK (UINT64_C(1) << 24)
#define WASI_RIGHT_PATH_REMOVE_DIRECTORY (UINT64_C(1) << 25)
#define WASI_RIGHT_PATH_UNLINK_FILE (UINT64_C(1) << 26)
#define WASI_RIGHT_POLL_FD_READWRITE (UINT64_C(1) << 27)
#define WASI_RIGHT_SOCK_SHUTDOWN (UINT64_C(1) << 28)
#define WASI_RIGHT_SOCK_ACCEPT (UINT64_C(1) << 29)

#define WASI_RIFLAG_RECV_PEEK (UINT16_C(1) << 0)
#define WASI_RIFLAG_RECV_WAITALL (UINT16_C(1) << 1)

#define WASI_ROFLAG_RECV_DATA_TRUNCATED (UINT16_C(1) << 0)

#define WASI_SDFLAG_RD (UINT16_C(1) << 0)
#define WASI_SDFLAG_WR (UINT16_C(1) << 1)

struct wasi_fdstat {
        uint8_t fs_filetype;
        uint8_t pad1;
        uint16_t fs_flags;
        uint8_t pad2[4];
        uint64_t fs_rights_base;
        uint64_t fs_rights_inheriting;
};
_Static_assert(sizeof(struct wasi_fdstat) == 24, "wasi_fdstat");
#define WASI_FDSTAT_ALIGN WASI_U64_ALIGN

struct wasi_fd_prestat {
        uint8_t type;
        uint8_t pad[3];
        uint32_t dir_name_len;
};
_Static_assert(sizeof(struct wasi_fd_prestat) == 8, "wasi_fd_prestat");
#define WASI_PRESTAT_ALIGN WASI_U32_ALIGN

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
#define WASI_FILESTAT_ALIGN WASI_U64_ALIGN

#define WASI_CLOCK_ID_REALTIME 0
#define WASI_CLOCK_ID_MONOTONIC 1
#define WASI_CLOCK_ID_PROCESS_CPUTIME_ID 2
#define WASI_CLOCK_ID_THREAD_CPUTIME_ID 3

#define WASI_SUBCLOCKFLAG_ABSTIME 1

struct wasi_subscription_clock {
        uint32_t clock_id; /* WASI_CLOCK_ID_ */
        uint64_t timeout;
        uint64_t precision;
        uint16_t flags; /* WASI_SUBCLOCKFLAG_ */
};
_Static_assert(sizeof(struct wasi_subscription_clock) == 32,
               "wasi_subscription_clock");

struct wasi_subscription_fd_readwrite {
        uint32_t fd;
};
_Static_assert(sizeof(struct wasi_subscription_fd_readwrite) == 4,
               "wasi_subscription_fd_readwrite");

struct wasi_subscription {
        uint64_t userdata;
        uint8_t type; /* WASI_EVENTTYPE_ */
        union {
                struct wasi_subscription_clock clock;
                struct wasi_subscription_fd_readwrite fd_read;
                struct wasi_subscription_fd_readwrite fd_write;
        } u;
};
_Static_assert(sizeof(struct wasi_subscription) == 48, "wasi_subscription");
#define WASI_SUBSCRIPTION_ALIGN WASI_U64_ALIGN

#define WASI_EVENTTYPE_CLOCK 0
#define WASI_EVENTTYPE_FD_READ 1
#define WASI_EVENTTYPE_FD_WRITE 2

#define WASI_EVENTRWFLAG_HANGUP 1

struct wasi_event {
        uint64_t userdata;
        uint16_t error;
        uint8_t type; /* WASI_EVENTTYPE_ */
        uint64_t availbytes;
        uint16_t rwflags; /* WASI_EVENTRWFLAG_ */
};
_Static_assert(sizeof(struct wasi_event) == 32, "wasi_event");
#define WASI_EVENT_ALIGN WASI_U64_ALIGN

#define WASI_DIRCOOKIE_START 0

struct wasi_dirent {
        uint64_t d_next;
        uint64_t d_ino;
        uint32_t d_namlen;
        uint8_t d_type;
};
_Static_assert(sizeof(struct wasi_dirent) == 24, "wasi_dirent");
#define WASI_DIRENT_ALIGN WASI_U64_ALIGN
