#include "utility.h"
#include "queue.h"

#ifndef _AGENT_MESSAGE_HANDLER_H
#define _AGENT_MESSAGE_HANDLER_H

typedef struct Assembled_Message{
	int type;
	int last_fragment_index;
	int total_message_size;
	int current_message_size;
	char* complete_message;
} Assembled_Message;

STATIC int parse_first_fragment(Queue_Message*, Assembled_Message*);

STATIC int parse_next_fragment(Queue_Message*, Assembled_Message*);

void* agent_handle_message(void*);

#endif /* _AGENT_MESSAGE_HANDLER_H */