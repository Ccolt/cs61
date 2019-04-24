#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern char* gets(char* buffer);

int main() {
    char buffer[100];
    if (gets(buffer)) {
        printf("Read %zu character(s)\n", strlen(buffer));
    } else {
        printf("Read nothing\n");
    }
}
