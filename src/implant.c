#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include <sys/socket.h>

#include "utility.h"
#include "implant.h"

int sendall(int fd, char *buf, int len){
    int total = 0;
    int bytesleft = len;
    int n;

    while(total < len) {
        n = send(fd, buf+total, bytesleft, 0);
        if (n == RET_ERROR) { break; }
        total += n;
        bytesleft -= n;
    }

    return (n == RET_ERROR) ? RET_ERROR : RET_OK;
}

int writeall(FILE* fd, char *buf, int len){
    int total = 0;
    int num, n;

    while(total < len) {
        num = (len - total >= WRITE_SIZE) ? WRITE_SIZE: (len - total);
        n = fwrite(buf + total, num, 1, fd);
        if (n != 1) { break; }
        
        total += num;
    }

    return (n == RET_ERROR) ? RET_ERROR : RET_OK;
}

int check_directory(char* pathname, int len){
    struct stat sb;
    char* slash_ptr;
    const char slash = '/';
    char* directory_path;

    if (pathname == NULL){
        return RET_ERROR;
    }

    directory_path = malloc(len);
    memcpy(directory_path, pathname, len);
    slash_ptr = strrchr(directory_path, slash);

    if (slash_ptr == NULL){
        // no directory included. Put file in current working directory
        return RET_OK;
    }

    *slash_ptr = 0;

    // check if directory is valid
    if (stat(directory_path, &sb) == 0 && S_ISDIR(sb.st_mode))
    {
        return RET_OK;
    }

    return RET_ERROR;
}