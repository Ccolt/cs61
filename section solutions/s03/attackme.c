#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern char* gets(char* buffer);

char *read_line(void) {
    char buffer[100];
    return gets(buffer);
}

int main() {
    char* buffer = read_line();
    if (buffer) {
        printf("Read %zu character(s)\n", strlen(buffer));
    } else {
        printf("Read nothing\n");
    }
}
