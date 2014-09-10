#ifndef _SERV_COMMON_H
#define _SERV_COMMON_H

typedef struct {
	int id;
	int fd;
} thread_arg;

#define SERV_PORT 9876
#define MAX_LINE  4096

#endif
