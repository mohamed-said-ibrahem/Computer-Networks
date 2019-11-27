#include <stdio.h>
#include <stdlib.h>

void DieWithError(char *errorMessage) {
    perror(errorMessage);
}
