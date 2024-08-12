#define BUFFERSIZE 4096

#define TYPE_ALIVE		0
#define TYPE_COMMAND	1
#define TYPE_PUT_FILE	2
#define TYPE_GET_FILE	3

#ifndef FRAGMENT_STRUCTURE
#define FRAGMENT_STRUCTURE

typedef struct Fragment{
	int32_t type;						// type of message (0 for alive, 1 for command)
	int32_t index;						// index of fragment in complete message
	char payload[BUFFERSIZE];			// payload
} Fragment;

#endif