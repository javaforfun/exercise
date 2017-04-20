#ifndef SERV_WORKER_H_
#define SERV_WORKER_H_

typedef struct {
    int id;
    int fd;
} thread_arg;

void *worker(void *arg);

#endif  // SERV_WORKER_H_
