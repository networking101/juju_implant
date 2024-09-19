#include <stdint.h>
#include <assert.h>

#ifndef _IMPLANT_H
#define _IMPLANT_H

#define STDIN   0
#define STDOUT  1
#define STDERR  2

#define TYPE_ALIVE			0
#define TYPE_COMMAND		1
#define TYPE_PUT_FILE_NAME	2
#define TYPE_PUT_FILE		3
#define TYPE_GET_FILE_NAME	4
#define TYPE_GET_FILE		5

#define RET_ERROR	-1
#define RET_OK		0
#define RET_ORDER	1

#define FRAGMENT_SIZE		4096
#define PAYLOAD_SIZE		4076
#define INITIAL_SIZE		20

#define AGENT_TIMEOUT	60

// macro to return size of structure field
#define member_size(type, member)	(sizeof( ((type *)0)->member))


typedef struct Fragment{
	int32_t type;						// type of message (0 for alive, 1 for command)
	int32_t index;						// index of fragment
	int32_t total_size;					// size of complete message, will be 0 for alive message
	int32_t next_size;					// size of next fragment
	union{
		uint32_t checksum;				// checksum of complete message, not present in alive messages
		uint32_t alive_time;			// if this is an alive message, include the epoch time of when agent was started on target
	};
	char buffer[PAYLOAD_SIZE];			// contents of message
} Fragment;

// // Make sure that we have the correct payload sizes defined
// // Check PAYLOAD_SIZE
// static_assert(FRAGMENT_SIZE == PAYLOAD_SIZE + member_size(Fragment, type) + member_size(Fragment, index) + member_size(Fragment, total_size) + member_size(Fragment, next_size) + member_size(Fragment, checksum));
// static_assert(FRAGMENT_SIZE == PAYLOAD_SIZE + member_size(Fragment, type) + member_size(Fragment, index) + member_size(Fragment, total_size) + member_size(Fragment, next_size) + member_size(Fragment, alive_time));
// // Check INITIAL_SIZE
// static_assert(INITIAL_SIZE == member_size(Fragment, type) + member_size(Fragment, index) + member_size(Fragment, total_size) + member_size(Fragment, next_size) + member_size(Fragment, checksum));
// static_assert(INITIAL_SIZE == member_size(Fragment, type) + member_size(Fragment, index) + member_size(Fragment, total_size) + member_size(Fragment, next_size) + member_size(Fragment, alive_time));

#endif /* _IMPLANT_H */