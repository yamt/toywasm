/* clang-format off */

/* args */
WASI_API(args_get, "(ii)i")
WASI_API(args_sizes_get, "(ii)i")

/* clock */
WASI_API(clock_res_get, "(ii)i")
WASI_API(clock_time_get, "(iIi)i")

/* environ */
WASI_API(environ_get, "(ii)i")
WASI_API(environ_sizes_get, "(ii)i")

/* fd */
WASI_API(fd_advise, "(iIIi)i")
WASI_API(fd_allocate, "(iII)i")
WASI_API(fd_close, "(i)i")
WASI_API(fd_datasync, "(i)i")
WASI_API(fd_fdstat_get, "(ii)i")
WASI_API(fd_fdstat_set_flags, "(ii)i")
WASI_API(fd_fdstat_set_rights, "(iII)i")
WASI_API(fd_filestat_get, "(ii)i")
WASI_API(fd_filestat_set_size, "(iI)i")
WASI_API(fd_filestat_set_times, "(iIIi)i")
WASI_API(fd_pread, "(iiiIi)i")
WASI_API(fd_prestat_dir_name, "(iii)i")
WASI_API(fd_prestat_get, "(ii)i")
WASI_API(fd_pwrite, "(iiiIi)i")
WASI_API(fd_read, "(iiii)i")
WASI_API(fd_readdir, "(iiiIi)i")
WASI_API(fd_renumber, "(ii)i")
WASI_API(fd_seek, "(iIii)i")
WASI_API(fd_sync, "(i)i")
WASI_API(fd_tell, "(ii)i")
WASI_API(fd_write, "(iiii)i")

/* path */
WASI_API(path_create_directory, "(iii)i")
WASI_API(path_filestat_get, "(iiiii)i")
WASI_API(path_filestat_set_times, "(iiiiIIi)i")
WASI_API(path_link, "(iiiiiii)i")
WASI_API(path_open, "(iiiiiIIii)i")
WASI_API(path_readlink, "(iiiiii)i")
WASI_API(path_remove_directory, "(iii)i")
WASI_API(path_rename, "(iiiiii)i")
WASI_API(path_symlink, "(iiiii)i")
WASI_API(path_unlink_file, "(iii)i")

/* poll */
WASI_API(poll_oneoff, "(iiii)i")

/* proc */
WASI_API(proc_exit, "(i)")

/* random */
WASI_API(random_get, "(ii)i")

/* sched */
WASI_API(sched_yield, "()i")

/* sock */
WASI_API(sock_accept, "(iii)i")
WASI_API(sock_recv, "(iiiiii)i")
WASI_API(sock_send, "(iiiii)i")
WASI_API(sock_shutdown, "(ii)i")

/* clang-format on */
