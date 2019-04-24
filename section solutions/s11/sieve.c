#include "helpers.h"

void add_filter(char* const* argv) {
    int pipefds[2];
    assert(pipe(pipefds) >= 0);
    int pid = fork();
    assert(pid != -1);
    if (pid == 0) {
        // do the child things
        dup2(pipefds[1], STDOUT_FILENO);
        close(pipefds[0]);
        close(pipefds[1]);
        execvp(argv[0], argv);
    }
    else {
        // do the parent things
        dup2(pipefds[0], STDIN_FILENO);
        close(pipefds[0]);
        close(pipefds[1]);
    }
}

int main(void) {
    // We never want buffering on stdin or stdout.
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    // sieve of Eratosthenes!
    // Start with a sequence of all numbers, starting from 2
    {
        char* argv[] = { "seq", "2", "1000000", NULL };
        add_filter(argv);
    }

    // Repeatedly:
    // 1. Read a number (the next prime)
    // 2. Cross off all multiples of that number by
    //    introducing another filter
    int last_prime;
    while (scanf("%d", &last_prime) == 1) {
        printf("%d\n", last_prime);

        char buf[20];
        sprintf(buf, "%d", last_prime);
        char* argv[] = { "./filtermultiples", buf, NULL };
        add_filter(argv);
    }
}
