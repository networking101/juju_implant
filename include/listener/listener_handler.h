#ifndef _MESSAGE_HANDLER_H
#define _MESSAGE_HANDLER_H

void *listener_handler_thread(void*);

int listener_prepare_message(int, int, char*, int);

#endif /* _MESSAGE_HANDLER_H */