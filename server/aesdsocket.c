#include <stdio.h>	
#include <stdlib.h> //malloc, free, exit
#include <netdb.h>	// 
#include <arpa/inet.h> // inet_ntop
#include <string.h>	// memset
#include <unistd.h> //close
#include <syslog.h>
#include <fcntl.h> // open(), flags O_CREAT ..
#include <signal.h>
#include <pthread.h>
#include "queue.h"
#include <stdatomic.h>

#define PORT "9000"
#define BUF_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"

// Global variables
volatile sig_atomic_t stop_requested = 0;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Local data types
typedef struct thread_slist_s thread_slist_t;
struct thread_slist_s
{
	pthread_t tid;
	atomic_int thread_done;
	SLIST_ENTRY(thread_slist_s) entries;
};

typedef struct 
{
	int client_fd;
	char ipstr[INET6_ADDRSTRLEN];
	atomic_int *p_thread_done;
}thread_args_t;


/*Private function declarations*/
int get_client_ip(struct sockaddr_storage client_addr, char* ipstr, size_t ipstr_len);
void handle_signal( int signo );
void *thread_handle_client(void *arg);
void *thread_timestamp(void *arg);

int main ( int argc, char *argv[])
{
	openlog( "aesdsocket", LOG_PID | LOG_CONS, LOG_USER );

	int d_mode = 0;
	int opt;

	while ((opt = getopt(argc, argv, "d")) != -1)
	{
		switch (opt)
		{
		case 'd':
			d_mode = 1;
			break;
		
		default:
			printf("Usage: %s [-d]\n", argv[0]);
			exit(1);
		}
	}
	

	struct addrinfo hints;
	struct addrinfo *p_res;

	// setting up the hints
	memset(&hints, 0, sizeof(hints));
	hints.ai_family 	= AF_UNSPEC;		// do not care ipv4 or 6
	hints.ai_socktype 	= SOCK_STREAM;		// TCP stream socket
	hints.ai_flags 		= AI_PASSIVE;		// fill in my IP for me

	// getting address info
	int status = getaddrinfo(NULL, PORT, &hints, &p_res);
	if (status != 0)
	{
		printf("getaddrinfo error: %s", gai_strerror(status));
		exit(1);
	}

	// create a socket
	int sock_fd = socket(p_res->ai_family, p_res->ai_socktype, p_res->ai_protocol);
	if (sock_fd == -1)
	{
		printf("failed to create a socket");
		freeaddrinfo(p_res);
		exit(1);
	}

	//TODO: Allow address reuse.
	int optval = 1;
	if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
	{
		printf("failed to allow address reuse");
		freeaddrinfo(p_res);
		close(sock_fd);
		exit(1);
	}

	if (bind(sock_fd, p_res->ai_addr, p_res->ai_addrlen) == -1)
	{
		printf("failed to bind the socket\n");
		freeaddrinfo(p_res);
		close(sock_fd);
		exit(1);
	}

	printf("socket was created and bound to port %s \n", PORT);
	freeaddrinfo(p_res);

	// release the daemon, if requested.
	if (d_mode)
	{
		pid_t pid = fork();
		if (pid < 0)
		{
			printf("fialed to fork");
			close(sock_fd);
			exit(1);
		}
		if (pid > 0)
		{
			// exit parent.
			exit(0);
		}
		
		// create a new session, detaches from terminal
		if (setsid() < 0)
		{
			printf("setsid failed\n");
			close(sock_fd);
			exit(1);
		}

		// change dir to avoid locking local dir.
		chdir("/");

		// redirect output/in , daemon should not use terminal.
		freopen("/dev/null", "r", stdin);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
	}

	// start timestamp thread after forking.
	pthread_t ts_tid;
	pthread_create(&ts_tid, NULL, thread_timestamp, NULL);

	if (listen(sock_fd, 5) < 0)
	{
		printf("listen failed\n");
		close(sock_fd);
		exit(1);
	}

	// register signal handler before goint into while
	struct sigaction sa = {0};
	sa.sa_handler = handle_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	// initialize the linked list
	SLIST_HEAD(thread_slist_head, thread_slist_s)  thread_list;
	SLIST_INIT(&thread_list);

	while (!stop_requested)
	{
		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
		if (stop_requested) break;
		if (client_fd<0)
		{
			printf("accept failed\n");
			close(sock_fd);
			exit(1);
		}

		char ipstr[INET6_ADDRSTRLEN];
		int res = get_client_ip(client_addr, ipstr, INET6_ADDRSTRLEN);
		if (stop_requested) break;
		if (res != 0)
		{
			close(sock_fd);
			close(client_fd);
			exit(1);	
		}
		
		syslog(LOG_INFO, "Accepted connection from %s", ipstr);
		printf("Accepted connection from %s\n", ipstr);

		// thread to handle recv & send from connected client.
		pthread_t tid;
		// package the args that will be sent to thread in a struct
		thread_args_t * p_thread_args = malloc(sizeof(thread_args_t));
		p_thread_args->client_fd = client_fd;
		memcpy(&(p_thread_args->ipstr), &ipstr, INET6_ADDRSTRLEN);

		// create a thread node, a pointer to thread done will be sent to thread
		// so thread will be able to tell when it is done.
		thread_slist_t *p_new_node = malloc(sizeof(thread_slist_t));
		if ( p_new_node == NULL )
		{
			printf("failed to malloc new node for thread id");
		}
		p_new_node->thread_done = 0;
		p_thread_args->p_thread_done = &(p_new_node->thread_done);

		// TODO: you need to send ipstr to thread to be able to log closing connection.
		if (pthread_create(&tid, NULL, thread_handle_client, p_thread_args) == 0)
		{
			// save tid to linked list.
			p_new_node->tid = tid;
			SLIST_INSERT_HEAD(&thread_list, p_new_node, entries);
		}
		else
		{
			free(p_new_node);
			free(p_thread_args);
			close(client_fd);
			printf("failed to create a thread.");
		}
		
		// iterate through the thread list and join finished threads.
		thread_slist_t *p_node;
		thread_slist_t *p_tmp_node;
		SLIST_FOREACH_SAFE(p_node, &thread_list, entries, p_tmp_node)
		{
			if(p_node->thread_done == 1)
			{
				pthread_join(p_node->tid, NULL);
				SLIST_REMOVE(&thread_list, p_node, thread_slist_s, entries);
				free(p_node);
			}
		}
	}

	// on signal, join all threads, remove data file and close log.

	thread_slist_t *p_node;
	thread_slist_t *p_tmp_node;

	SLIST_FOREACH_SAFE(p_node, &thread_list, entries, p_tmp_node)
	{
		pthread_join(p_node->tid, NULL);
		SLIST_REMOVE(&thread_list, p_node, thread_slist_s, entries);
		free(p_node);
	}
	
	pthread_join(ts_tid, NULL);

	remove(DATA_FILE_PATH);
	closelog();
	return 0;
}

/*		***********************************
		Private functions defenition
		***********************************
*/

int get_client_ip(struct sockaddr_storage client_addr, char* ipstr, size_t ipstr_len)
{
	// obtaining the client ip address. 
	void *addr;

	if(client_addr.ss_family == AF_INET)
	{
		struct sockaddr_in *s = (struct sockaddr_in*) &client_addr;
		addr = &(s->sin_addr);
	}
	else if(client_addr.ss_family == AF_INET6)
	{
		struct sockaddr_in6 *s = (struct sockaddr_in6*)&client_addr;
		addr = &(s->sin6_addr);
	}
	else
	{
		printf("Unknown client adress family\n");
		return -1;
	}
	inet_ntop(client_addr.ss_family, addr, ipstr, ipstr_len);
	return 0;
}

void handle_signal( int signo )
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_INFO, "Caught signal, exiting");
		printf("Caught signal, exiting\n");
		stop_requested = 1;
	}
}

void *thread_handle_client(void *arg)
{
	thread_args_t *p_thread_args = ((thread_args_t*)arg);
	char buffer[BUF_SIZE];
	int data_fd= open(DATA_FILE_PATH, O_CREAT|O_WRONLY|O_APPEND, 0644);
	if(data_fd == -1 )
	{
		printf("failed to open data file");
		pthread_exit(NULL);
	}

	// receive until new line
	int bytes_read= 0;
	
	while ((bytes_read = recv(p_thread_args->client_fd, buffer, BUF_SIZE, 0)) > 0)
	{
		pthread_mutex_lock(&file_mutex);
		write(data_fd, buffer, bytes_read);
		pthread_mutex_unlock(&file_mutex);
		if(memchr(buffer, '\n', bytes_read)) break;
	}
	
	close(data_fd);

	// send collected data
	data_fd = open(DATA_FILE_PATH, O_RDONLY);
	while ((bytes_read = read(data_fd, buffer, BUF_SIZE)) > 0)
	{
		send(p_thread_args->client_fd, buffer, bytes_read, 0);
	}
	close(data_fd);
	

	close(p_thread_args->client_fd);


	syslog(LOG_INFO, "Closed connection from %s", p_thread_args->ipstr);
	printf("Closed connection from %s\n", p_thread_args->ipstr);

	*(p_thread_args->p_thread_done) = 1;
	free(p_thread_args);
	return NULL;
}

void *thread_timestamp(void *arg)
{
	(void)arg;
	time_t last_stamp = time(NULL);
	while (!stop_requested)
	{
		sleep(1);
		time_t now = time(NULL);

		if (difftime(now, last_stamp) >= 10.0)
		{
			last_stamp = now;
			struct tm *time_info = localtime(&now);
			char timestamp[64]= {0};
			strftime(timestamp, sizeof(timestamp), "timestamp: %Y-%m-%d %H:%M:%S\n", time_info);

			int data_fd= open(DATA_FILE_PATH, O_CREAT|O_WRONLY|O_APPEND, 0644);
			if(data_fd == -1 )
			{
				printf("failed to open data file for writing timestamp");
				pthread_exit(NULL);
			}
			pthread_mutex_lock(&file_mutex);
			write(data_fd, timestamp, strlen(timestamp));
			printf("%s", timestamp);
			pthread_mutex_unlock(&file_mutex);
			close(data_fd);
		}
	}
	return NULL;
}
