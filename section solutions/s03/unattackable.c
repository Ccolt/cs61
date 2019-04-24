#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* read_line(char* buffer, size_t sz) {
    // 1. Take a `buffer` argument so as not to return a pointer to
    //    a local variable!
    // 2. Use `fgets`, which never reads more than `sz-1` characters.
    return fgets(buffer, sz, stdin);
}

int main() {
    char buffer[100];
    if (read_line(buffer, sizeof(buffer))) {
        printf("Read %zu character(s)\n", strlen(buffer));
    } else {
        printf("Read nothing\n");
    }
}
