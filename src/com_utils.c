#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "com_utils.h"

int64_t read_64(FILE* file)
{
    char buff[32];
    if(fgets(buff, 32, file) == NULL){
        return 0;
    }
    return atol(buff);
}