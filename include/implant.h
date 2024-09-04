#include <stdint.h>

#ifndef _IMPLANT_H
#define _IMPLANT_H

#define STDIN   0
#define STDOUT  1
#define STDERR  2

#define TYPE_ALIVE		0
#define TYPE_COMMAND	1
#define TYPE_PUT_FILE	2
#define TYPE_GET_FILE	3

#define RET_ERROR	-1
#define RET_OK		0

#define BUFFERSIZE		4096
#define FRAGMENTSIZE	BUFFERSIZE + 8
#define AGENT_TIMEOUT	60

typedef struct First_Payload{
	int32_t total_size;
	union {
		char actual_payload[BUFFERSIZE - 4];
		uint32_t alive_time;
	};
	
} First_Payload;

typedef struct Fragment{
	int32_t type;						// type of message (0 for alive, 1 for command)
	int32_t index;						// index of fragment in complete message
	union {
		char next_payload[BUFFERSIZE];			// payload
		First_Payload first_payload;
	};
} Fragment;



#endif /* _IMPLANT_H */