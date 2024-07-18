#ifndef IMPLANT_POLL_STRUCTURE
#define IMPLANT_POLL_STRUCTURE

struct implant_poll{
    int *fd_count;
    struct pollfd *pfds;
};

#endif


#ifndef MESSAGE_STRUCTURE
#define MESSAGE_STRUCTURE

struct Message{
	int sockfd;
	char* buffer;
};

#endif