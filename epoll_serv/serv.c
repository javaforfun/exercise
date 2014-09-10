/*
 * main thread start listen socket, loop for accept, and pass new connected fd
 * to worker thread.
 *
 * worker thread use epoll for pipe and socket event notification.
 * pipe mesg is new connected fd which send by main thread,
 * worker thread then register this new conn_fd to epoll.
 *
 * assume CPU num is n, this little server will start n worker threads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "common.h"
#include "worker.h"

int start_listen()
{
	int fd, opt = 1;
	struct sockaddr_in servaddr;

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("open socket failed!\n");
		exit(1);
	}

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if ((bind(fd, (struct sockaddr*) &servaddr, sizeof(servaddr))) < 0) {
		printf("bind failed!\n");
		exit(1);
	}

	if (listen(fd, SOMAXCONN) < 0) {
		printf("listen failed!\n");
		exit(1);
	}

	return fd;
}

int main(int argc, char **argv)
{
	int i, num_cores, listen_fd, conn_fd;
	char name[16];
	pthread_attr_t attr;

	listen_fd = start_listen();

	num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	printf("core num: %d\n", num_cores);

	int pipe_fd[num_cores][2];
	thread_arg targ[num_cores];
	pthread_t tid[num_cores];

	if (pthread_attr_init(&attr) != 0) {
		perror("pthrad attr init error: ");
		exit(1);
	}

	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
		perror("pthread set attr detached error: ");
		exit(1);
	}

	for (i = 0; i < num_cores; i++) {
		pipe(pipe_fd[i]);
		targ[i] = (thread_arg) {i, pipe_fd[i][0]};

		if (pthread_create(&tid[i], &attr, worker, &targ[i]) != 0) {
			perror("pthread create error: ");
			exit(1);
		}
	}

	pthread_attr_destroy(&attr);
	sleep(2);
	printf("server started\n\n");

	// dispatch conn_fd to worker thread
	while ((conn_fd = accept(listen_fd, NULL, NULL)) > 0) {
		sprintf(name, "%d", conn_fd);
		i = conn_fd % num_cores;
		write(pipe_fd[i][1], name, strlen(name));
	}

	close(listen_fd);

	return 0;
}

/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */
