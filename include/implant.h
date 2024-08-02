#ifndef MESSAGE_STRUCTURE
#define MESSAGE_STRUCTURE

struct Message{
	int id;
	uint size;
	char* buffer;
};

#define BUFFERSIZE 4096

#endif