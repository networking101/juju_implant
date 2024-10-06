#include <stdint.h>
#include <assert.h>

#ifndef _IMPLANT_H
#define _IMPLANT_H

// File descriptors
#define STDIN				0
#define STDOUT				1
#define STDERR				2

// Socket message types
#define TYPE_ALIVE			0
#define TYPE_COMMAND		1
#define TYPE_PUT_FILE_NAME	2
#define TYPE_PUT_FILE		3
#define TYPE_GET_FILE_NAME	4
#define TYPE_GET_FILE		5
#define TYPE_CLOSE_AGENT	6
#define TYPE_RESPONSE		7

// Return values
#define RET_FATAL_ERROR		-1	// This error is bad juju. Kill program
#define RET_OK				0	// This return value is ok
#define RET_ERROR			1	/* This value is not good but should not kill
								the program. Usually indicates that the calling
								function needs to handle return value.*/ 

// Buffer sizes
#define FILE_WRITE_SIZE		4096
#define FRAGMENT_SIZE		1024
#define PAYLOAD_SIZE		1004
#define HEADER_SIZE			20

// Timeouts
#define AGENT_TIMEOUT		60				// 60 s
#define ALIVE_FREQUENCY		10				// 10 s
#define MS_TIMEOUT			1000			// 1000 ms
#define S_TIMEOUT  			1				// 1 s

// Status messages
typedef struct Status_Message{
	int index;
	const char *msg;
} Status_Message;

#define STATUS_ERROR				0
#define STATUS_MESSAGE_ORDER		1
#define STATUS_FILE_EXIST			2
#define STATUS_PATH_EXIST			3
#define STATUS_FILE_ACCESS			4
#define STATUS_MESSAGE_INCOMPLETE	5
#define STATUS_BAD_CRC				6
#define STATUS_UNKNOWN_MESSAGE		7
#define STATUS_FILE_PUT				8

extern Status_Message message_map[];

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
		int32_t response_id;			// if this is a response message, include the id
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

#endif /* _IMPLANT_H */