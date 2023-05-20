#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int main() {
    char* bug = calloc(4, 1);
    strcpy(bug, "bug");
    bug = realloc(bug, 6);
    strcat(bug, "gy");
    printf(bug);
    return 0;
}
