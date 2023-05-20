#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main() {
    char* bug = calloc(6, 1);
    strcpy(bug, "buggy");
    char* spot = bug + 3;
    bug[2] = '\0';
    printf("bug: %s\n", bug);
    printf("spot: %s\n", spot);
    realloc(bug, 5);
    bug[2] = 'n';
    bug[3] = 'o';
    bug[4] = '\0';
    printf("bug: %s\n", bug);
    printf("spot: %s\n", spot);


    return 0;
}
