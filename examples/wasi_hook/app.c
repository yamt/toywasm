#include <stdio.h>
#include <unistd.h>

int
main()
{
        char buf[100];
        ssize_t len = read(STDIN_FILENO, buf, sizeof(buf));
        printf("hello, \"%.*s\"\n", (int)len, buf);
}
