#include <stdio.h>
#include <stdlib.h>

#include "console.h"

#define BUFFERSIZE 10

// Global variables
extern struct implant_poll *poll_struct;

int agent_console(){
	
	return 0;
}


void *console(void *vargp){
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;
	
	char* menu = "Welcome to the implant listener.\n"
				 "Provide an option.\n"
				 "1 - list active implants\n"
				 "2 - select active implant\n"
				 "3 - get processes (ps -aux)\n"
				 "4 - get network statistics (netstat -plantu)\n";
	char buffer[BUFFERSIZE];
	long option;
				 
	printf("%s\n", menu);
	
	while(fgets(buffer, BUFFERSIZE, stdin)){
		option = strtol(buffer, NULL, 10);
		switch(option){
            case 1:
            	for (int i = 0; i < *fd_count; i++){
					printf("Implant %d\n", i+1);
				}
            	break;
            case 2:
            	break;
            case 3:
            	break;
            default:
            	printf("Unknown option selected\n");
            	break;
        }
	}
	return 0;
}