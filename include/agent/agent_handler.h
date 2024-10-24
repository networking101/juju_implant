#ifndef _AGENT_MESSAGE_HANDLER_H
#define _AGENT_MESSAGE_HANDLER_H

#include "utility.h"
#include "queue.h"

typedef struct Assembled_Message{
	Fragment_Header last_header;
	int current_message_size;
	char* file_name;
	char* message;
} Assembled_Message;

void *agent_handler_thread(void*);

int agent_prepare_response(int);

int agent_prepare_message(int, char*, int);

#endif /* _AGENT_MESSAGE_HANDLER_H */