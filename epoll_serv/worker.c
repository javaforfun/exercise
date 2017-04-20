#include "worker.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_FDS 1000000
#define MAX_EVENTS 1000
#define MAX_LINE 1024

extern volatile int thread_num;

int taskset_thread_core(int core_id);
int setnonblocking(int fd);
void handle_req(int conn_fd);
void run_epoll(int epfd, int pipe_fd);

int taskset_thread_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t tid = pthread_self();
    return pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
}

int setnonblocking(int fd) {
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK) == -1) {
        printf("fd %d set non blocking failed\n", fd);
        return -1;
    }
    return 0;
}

void handle_req(int conn_fd) {
    char in_buff[MAX_LINE] = {0};
    int n = 0;
    while ((n = recv(conn_fd, in_buff, MAX_LINE, 0)) > 0) {
        in_buff[n] = '\0';
        printf("recv:%s\n", in_buff);
        send(conn_fd, in_buff, n, 0);
    }

    if (n < 0 && errno == EAGAIN)
        return;
    if (n < 0)
        printf("read error: %d, %d\n", conn_fd, errno);
    
    printf("close: %d\n", conn_fd);
    close(conn_fd);  // epoll remove fd automaticly
}

/*
 * use epoll loop pipe and socket msg. if a pipe msg recv,
 * it means we got a new connection, register this conn_fd to epoll
 */
void run_epoll(int epfd, int pipe_fd) {
    int i, nfds;
    char buff[16];
    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;

    while (1) {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (i = 0; i < nfds; ++i) {
            // pipe msg, add connected fd to epoll
            if (events[i].data.fd == pipe_fd) {
                while (read(pipe_fd, buff, 16) > 0) {
                    int conn_fd = atoi(buff);
                    setnonblocking(conn_fd);
                    ev.data.fd = conn_fd;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev) < 0) {
                        perror("epoll_ctl add conn_fd failed: ");
                    }
                    printf("accept:%d\n", conn_fd);
                }
            } else {  // socket msg
                if (events[i].events & EPOLLRDHUP) {
                    printf("rdhup: %d\n", events[i].data.fd);
                    close(events[i].data.fd);
                } else {
                    handle_req(events[i].data.fd);
                }
            }
        }
    }
}

void *worker(void *arg) {
    int epfd, pipe_fd;
    struct epoll_event ev;

    // bind thread to certain CPU core
    taskset_thread_core(((thread_arg*) arg)->id);

    // create epoll instance, register pipe_fd on it
    pipe_fd = ((thread_arg*) arg)->fd;
    epfd = epoll_create(MAX_FDS);
    setnonblocking(pipe_fd);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = pipe_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_fd, &ev) < 0) {
        printf("epoll add mq fail\n");
    }
    thread_num++;

    run_epoll(epfd, pipe_fd);
    return 0;
}
