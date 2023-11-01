
struct runwasi_cli_args {
        const char *filename;
        unsigned int ndirs;
        char **dirs;
        unsigned int nenvs;
        char **envs;
        int argc;
        char **argv;
};

int runwasi_cli_args_parse(int argc, char **argv, struct runwasi_cli_args *a);
