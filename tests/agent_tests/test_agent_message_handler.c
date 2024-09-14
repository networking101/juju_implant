#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "queue.h"
#include "agent_handler.h"

#define ALIVE	"ALIVE"

// Test if parse_first_fragment() can handle a short message. Tested by setting up a fragment structure and initializing a fragment as a static character array.
static void test_parse_first_fragment_short_message(void **state){
	Fragment fragment = {0};
	Queue_Message q_message = {0};
	Assembled_Message a_message = {0};
	
	// Using structures
	fragment.type = 1;
	fragment.index = 0;
	int actual_payload_len = (int)strlen(ALIVE);
	fragment.first_payload.total_size = actual_payload_len;
	strcpy(fragment.first_payload.actual_payload, ALIVE);
	
	// size = type (4 bytes) + index (4 bytes) + payload
	//															-> final size (4 bytes) + length of message (5 bytes)  
	q_message.size = sizeof(fragment.type) + sizeof(fragment.index) + sizeof(fragment.first_payload.total_size) + actual_payload_len;
	q_message.fragment = &fragment;
	
	a_message.last_fragment_index = -1;
	
	int retval = parse_first_fragment(&q_message, &a_message);
	
	assert_int_equal(retval, RET_OK);
	assert_int_equal(a_message.type, fragment.type);
	assert_int_equal(a_message.last_fragment_index, 0);
	assert_int_equal(a_message.total_message_size, strlen(ALIVE));
	assert_int_equal(a_message.current_message_size, strlen(ALIVE));
	assert_memory_equal(a_message.complete_message, ALIVE, strlen(ALIVE));
	
	// TODO find out why this is segfaulting
//	free(a_message.complete_message);
	
	
	// Using static char buffer
	//						   |     type     |     index     |  final size   |      message      | 
	char static_fragment[]  = "\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x41\x4c\x49\x56\x45";
	memset(&q_message, 0, sizeof(Queue_Message));
	memset(&a_message, 0, sizeof(Assembled_Message));
	
	q_message.size = 17;
	q_message.fragment = (Fragment*)static_fragment;
	
	a_message.last_fragment_index = -1;
	
	retval = parse_first_fragment(&q_message, &a_message);
	
	assert_int_equal(retval, RET_OK);
	assert_int_equal(a_message.type, fragment.type);
	assert_int_equal(a_message.last_fragment_index, 0);
	assert_int_equal(a_message.total_message_size, strlen(ALIVE));
	assert_int_equal(a_message.current_message_size, strlen(ALIVE));
	assert_memory_equal(a_message.complete_message, ALIVE, strlen(ALIVE));
	
	// TODO find out why this is segfaulting
//	free(a_message.complete_message);
	return;
}
	
// Test if parse_first_fragment() can handle a long message.
static void test_parse_first_fragment_long_message(void **state){
	Fragment fragment = {0};
	Queue_Message q_message = {0};
	Assembled_Message a_message = {0};
	
	memset(fragment.first_payload.actual_payload, 0x41, FIRST_PAYLOAD_SIZE);
	
	// Using structures
	fragment.type = 1;
	fragment.index = 0;
	int actual_payload_len = (int)strlen(fragment.first_payload.actual_payload);
	fragment.first_payload.total_size = actual_payload_len + 10;			// We are indicating that the entire message cannot fit in 1 fragment
	
	// size = type (4 bytes) + index (4 bytes) + payload (4096)  
	q_message.size = sizeof(fragment.type) + sizeof(fragment.index) + sizeof(fragment.first_payload.total_size) + actual_payload_len;
	q_message.fragment = &fragment;
	
	a_message.last_fragment_index = -1;
	
	int retval = parse_first_fragment(&q_message, &a_message);
	
	assert_int_equal(retval, RET_OK);
	assert_int_equal(a_message.type, fragment.type);
	assert_int_equal(a_message.last_fragment_index, 0);
	assert_int_equal(a_message.total_message_size, a_message.current_message_size + 10);
	assert_int_equal(a_message.current_message_size, actual_payload_len);
	assert_memory_equal(a_message.complete_message, fragment.first_payload.actual_payload, strlen(fragment.first_payload.actual_payload));
	
//	free(a_message.complete_message);
	return;
}
	
	
// Test wrong fragment index. If the first fragment index is not 0. The function should return an error.
// Tested by setting up a fragment structure and initializing a fragment as a static character array.
static void test_parse_first_fragment_wrong_frame_index(void **state){
	Fragment fragment = {0};
	Queue_Message q_message = {0};
	Assembled_Message a_message = {0};
	
	// Using structures
	fragment.type = 1;
	fragment.index = 1;
	int actual_payload_len = (int)strlen(ALIVE);
	fragment.first_payload.total_size = actual_payload_len;
	strcpy(fragment.first_payload.actual_payload, ALIVE);
	
	// size = type (4 bytes) + index (4 bytes) + payload
	//															-> final size (4 bytes) + length of message (5 bytes)  
	q_message.size = sizeof(fragment.type) + sizeof(fragment.index) + sizeof(fragment.first_payload.total_size) + actual_payload_len;
	q_message.fragment = &fragment;
	
	a_message.last_fragment_index = -1;
	
	int retval = parse_first_fragment(&q_message, &a_message);
	
	assert_int_equal(retval, RET_ERROR);
	assert_int_equal(a_message.last_fragment_index, -1);
	
	
	// Using static char buffer
	//						   |     type     |     index     |  final size   |      message      | 
	char static_fragment[]  = "\x01\x00\x00\x00\x01\x00\x00\x00\x05\x00\x00\x00\x41\x4c\x49\x56\x45";
	memset(&q_message, 0, sizeof(Queue_Message));
	memset(&a_message, 0, sizeof(Assembled_Message));
	
	q_message.size = 17;
	q_message.fragment = (Fragment*)static_fragment;
	
	a_message.last_fragment_index = -1;
	
	retval = parse_first_fragment(&q_message, &a_message);
	
	assert_int_equal(retval, RET_ERROR);
	assert_int_equal(a_message.last_fragment_index, -1);

	return;
}

// Test if parse_first_fragment() can handle a short message. Tested by setting up a fragment structure and initializing a fragment as a static character array.
static void test_parse_next_fragment_short_message(void **state){
	Fragment fragment = {0};
	Queue_Message q_message = {0};
	Assembled_Message a_message = {0};
	
	char check_complete_message[FIRST_PAYLOAD_SIZE + 5 + 1] = {0};
	memset(check_complete_message, 0x41, FIRST_PAYLOAD_SIZE);
	memcpy(check_complete_message + FIRST_PAYLOAD_SIZE, ALIVE, strlen(ALIVE));
	
	// Using structures
	fragment.type = 1;
	fragment.index = 1;
	int this_payload_len = (int)strlen(ALIVE);
	strcpy(fragment.next_payload, ALIVE);
	
	// size = type (4 bytes) + index (4 bytes) + payload (5 bytes)  
	q_message.size = sizeof(fragment.type) + sizeof(fragment.index) + this_payload_len;
	q_message.fragment = &fragment;
	
	a_message.type = 1;
	a_message.last_fragment_index = 0;
	a_message.total_message_size = NEXT_PAYLOAD_SIZE - sizeof(fragment.first_payload.total_size) + strlen(ALIVE);
	a_message.current_message_size = NEXT_PAYLOAD_SIZE - sizeof(fragment.first_payload.total_size);
	a_message.complete_message = malloc(a_message.total_message_size);
	memset(a_message.complete_message, 0x41, FIRST_PAYLOAD_SIZE);
	
	int retval = parse_next_fragment(&q_message, &a_message);
	
	assert_int_equal(retval, RET_OK);
	assert_int_equal(a_message.type, fragment.type);
	assert_int_equal(a_message.last_fragment_index, 1);
	assert_int_equal(a_message.total_message_size, a_message.current_message_size);
	assert_memory_equal(a_message.complete_message, check_complete_message, a_message.total_message_size);
	
	free(a_message.complete_message);
	
	
	// Using static char buffer
	//						   |     type     |     index     |      message      | 
//	char static_fragment[]  = "\x01\x00\x00\x00\x01\x00\x00\x00\x41\x4c\x49\x56\x45";
//	memset(&q_message, 0, sizeof(Queue_Message));
//	memset(&a_message, 0, sizeof(Assembled_Message));
//	
//	q_message.size = 13;
//	q_message.fragment = (Fragment*)static_fragment;
//	
//	a_message.type = 1;
//	a_message.last_fragment_index = 0;
//	a_message.total_message_size = BUFFERSIZE - sizeof(fragment.first_payload.total_size) + strlen(ALIVE);
//	a_message.current_message_size = BUFFERSIZE - sizeof(fragment.first_payload.total_size);
//	a_message.complete_message = malloc(a_message.total_message_size);
//	memset(a_message.complete_message, 0x41, BUFFERSIZE - 4);
//	
//	retval = parse_next_fragment(&q_message, &a_message);
//	
//	assert_int_equal(retval, RET_OK);
//	assert_int_equal(a_message.type, fragment.type);
//	assert_int_equal(a_message.last_fragment_index, 1);
//	assert_int_equal(a_message.total_message_size, a_message.current_message_size);
//	assert_memory_equal(a_message.complete_message, check_complete_message);
//	
//	free(a_message.complete_message);
	return;
}
	
// Test if parse_first_fragment() can handle a long message.
static void test_parse_next_fragment_long_message(void **state){

	return;
}
	
	
// Test wrong fragment index. If the first fragment index is not 0, the function should return an error.
// Tested by setting up a fragment structure and initializing a fragment as a static character array.
static void test_parse_next_fragment_wrong_frame_index(void **state){

	return;
}

// Test the fragment type. If the fragment does not match the type of the previous fragments, the function should return an error.
static void test_parse_next_fragment_wrong_type(void **state){

	return;
}

int test_agent_message_handler(){
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_parse_first_fragment_short_message),
		cmocka_unit_test(test_parse_first_fragment_long_message),
		cmocka_unit_test(test_parse_first_fragment_wrong_frame_index),
		cmocka_unit_test(test_parse_next_fragment_short_message),
		cmocka_unit_test(test_parse_next_fragment_long_message),
		cmocka_unit_test(test_parse_next_fragment_wrong_frame_index),
		cmocka_unit_test(test_parse_next_fragment_wrong_type),
	};
	
	return cmocka_run_group_tests(tests, NULL, NULL);
}