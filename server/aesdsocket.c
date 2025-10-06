#include <stdio.h>	
#include <stdlib.h> //malloc, free, exit
#include <netdb.h>	// 
#include <arpa/inet.h> // inet_ntop
#include <string.h>	// memset
#include <unistd.h> //close
#include <syslog.h>
#include <fcntl.h> // open(), flags O_CREAT ..

#define PORT "9000"
#define BUF_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"

/*Private function declarations*/
int get_client_ip(struct sockaddr_storage client_addr, char* ipstr, size_t ipstr_len);

int main (void)
{
	openlog( "aesdsocket", LOG_PID | LOG_CONS, LOG_USER );
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
		free(p_res);
		exit(1);
	}

	//TODO: Allow address reuse.

	if (bind(sock_fd, p_res->ai_addr, p_res->ai_addrlen) == -1)
	{
		printf("failed to bind the socket\n");
		free(p_res);
		close(sock_fd);
		exit(1);
	}

	printf("socket was created and bound to port %s \n", PORT);
	free(p_res);

	if (listen(sock_fd, 5) < 0)
	{
		printf("listen failed\n");
		close(sock_fd);
		exit(1);
	}

	char buffer[BUF_SIZE];
	while (1)
	{
		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
		if (client_fd<0)
		{
			printf("accept failed\n");
			close(sock_fd);
			exit(1);
		}

		char ipstr[INET6_ADDRSTRLEN];
		if (get_client_ip(client_addr, ipstr, INET6_ADDRSTRLEN) != 0)
		{
			close(sock_fd);
			close(client_fd);
			exit(1);	
		}
		
		syslog(LOG_INFO, "Accepted connection from %s", ipstr);
		printf("Accepted connection from %s\n", ipstr);

		int data_fd= open(DATA_FILE_PATH, O_CREAT|O_WRONLY|O_APPEND, 0644);
		if(data_fd == -1 )
		{
			printf("failed to open data file");
			exit(1);
		}

		// receive until new line
		int bytes_read= 0;
		while ((bytes_read = recv(client_fd, buffer, BUF_SIZE, 0)) > 0)
		{
			write(data_fd, buffer, bytes_read);
			if(memchr(buffer, '\n', bytes_read)) break;
		}
		close(data_fd);

		// send collected data
		data_fd = open(DATA_FILE_PATH, O_RDONLY);
		while ((bytes_read = read(data_fd, buffer, BUF_SIZE)) > 0)
		{
			send(client_fd, buffer, bytes_read, 0);
		}
		close(data_fd);
		

		close(client_fd);
	}
	
	closelog();
	return 0;
}


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