#ifndef IMPLANT_POLL_STRUCTURE
#define IMPLANT_POLL_STRUCTURE

struct implant_poll{
    int *fd_count;
    struct pollfd *pfds;
};

#endif

int poll_add(int);

int poll_delete(int);