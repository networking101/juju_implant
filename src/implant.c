#include <stdio.h>
#include <stdlib.h>

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