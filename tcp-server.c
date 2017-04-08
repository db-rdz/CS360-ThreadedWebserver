#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include "http-parser.h"

#define BUFFER_MAX	1024

void parseConfig(char *filename);

void handle_sigchld(int sig);
int create_server_socket(char* port, int protocol);
void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len);

int init_tcp(char* path, char* port, int verbose) {

	verbose_flag = verbose;
	int sock = create_server_socket(port, SOCK_STREAM);
	parseConfig(path);

	while (1) {
		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		int client = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len);
		if (client == -1) {
			perror("Connection Failed at function: accept");
			continue;
		}
		else{
			pid_t pid = fork();
			if(pid == 0){
				handle_client(client, client_addr, client_addr_len);
			}
		}
	}
	return 0;
}

void resetParsingHeader(){
    parsing_request.is_header_ready = 0;
    parsing_request.is_body_ready = 0;
    parsing_request.content_length = 0;
    parsing_request.parsed_body = 0;
    parsing_request.responseFlag = 0;
    parsing_request.dynamicContent = NON_DYNAMIC;
    parsing_request.byte_range = 0;
    parsing_request.is_header_parsed = 0;
    parsing_request.is_body_parsed = 0;
    parsing_request.fragmented_line_waiting = 0;
}

void resetParsingHeaderFlags(){
    first_line_read = 0;
    header_index = 0;
}

void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len) {

	unsigned char buffer[BUFFER_MAX];
	char client_hostname[NI_MAXHOST];
	char client_port[NI_MAXSERV];

	int ret = getnameinfo((struct sockaddr*)&client_addr, addr_len, client_hostname,
		       NI_MAXHOST, client_port, NI_MAXSERV, 0);

	if (ret != 0) {
		fprintf(stderr, "Failed in getnameinfo: %s\n", gai_strerror(ret));
	}
	if(verbose_flag) printf("\nGot a connection from %s:%s\n", client_hostname, client_port);
	//You gotta receive until you see the double carriage return
	//After that you can check for content length and know if you are done or not.
    resetParsingHeader();
    resetParsingHeaderFlags();

	while (1) {
		int bytes_read = recv(sock, buffer, BUFFER_MAX-1, 0);
		if (bytes_read == 0) {
			if(verbose_flag) printf("Peer disconnected\n");
			close(sock);
            exit(EXIT_SUCCESS);
		}
		if (bytes_read < 0) {
			perror("recv");
			continue;
		}

		buffer[bytes_read] = '\0';
		if(verbose_flag) printf("RECEIVED:\n  %s\n", buffer);

        struct request r;

        if(isHeaderComplete(buffer) && !parsing_request.is_header_parsed){
            parsing_request.is_header_ready = 1;
            parseHeader(buffer, &parsing_request);
            parsing_request.is_header_parsed = 1;
        }
        else if(!parsing_request.is_header_parsed){
            //if the header isn't complete parse whatever you got and wait to receive more.
            parseHeader(buffer, &parsing_request);
            continue;
        }

        if(verbose_flag) printf("PARSED HEADER. REQUEST TYPE: %s\n", parsing_request.rl.type);

        if(parsing_request.rl.type[0] == 'P'){
            parseBody(buffer, &parsing_request);
            if(!parsing_request.is_body_parsed){
                continue;
            }
        }

        sanitize_path(&parsing_request);
        executeRequest(&parsing_request, sock);
        resetParsingHeader();
        resetParsingHeaderFlags();
	}
}

int create_server_socket(char* port, int protocol) {
	int sock;
	int ret;
	int optval = 1;
	struct addrinfo hints;
	struct addrinfo* addr_ptr;
	struct addrinfo* addr_list;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = protocol;
	/* AI_PASSIVE for filtering out addresses on which we
	 * can't use for servers
	 *
	 * AI_ADDRCONFIG to filter out address types the system
	 * does not support
	 *
	 * AI_NUMERICSERV to indicate port parameter is a number
	 * and not a string
	 *
	 * */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
	/*
	 *  On Linux binding to :: also binds to 0.0.0.0
	 *  Null is fine for TCP, but UDP needs both
	 *  See https://blog.powerdns.com/2012/10/08/on-binding-datagram-udp-sockets-to-the-any-addresses/
	 */
	ret = getaddrinfo(protocol == SOCK_DGRAM ? "::" : NULL, port, &hints, &addr_list);
	if (ret != 0) {
		fprintf(stderr, "Failed in getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}
	
	for (addr_ptr = addr_list; addr_ptr != NULL; addr_ptr = addr_ptr->ai_next) {
		sock = socket(addr_ptr->ai_family, addr_ptr->ai_socktype, addr_ptr->ai_protocol);
		if (sock == -1) {
			perror("socket");
			continue;
		}

		// Allow us to quickly reuse the address if we shut down (avoiding timeout)
		ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		if (ret == -1) {
			perror("setsockopt");
			close(sock);
			continue;
		}

		ret = bind(sock, addr_ptr->ai_addr, addr_ptr->ai_addrlen);
		if (ret == -1) {
			perror("bind");
			close(sock);
			continue;
		}
		break;
	}
	freeaddrinfo(addr_list);
	if (addr_ptr == NULL) {
		fprintf(stderr, "Failed to find a suitable address for binding\n");
		exit(EXIT_FAILURE);
	}

	if (protocol == SOCK_DGRAM) {
		return sock;
	}
	// Turn the socket into a listening socket if TCP
	ret = listen(sock, SOMAXCONN);
	if (ret == -1) {
		perror("listen");
		close(sock);
		exit(EXIT_FAILURE);
	}

	return sock;
}

