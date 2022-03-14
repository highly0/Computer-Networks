#include <argp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "hash.h"

struct server_arguments {
	struct sockaddr_in serv_addr;
	char *salt;
	size_t salt_len;
};
void payload(struct server_arguments args);
void send_num_bytes(int socket, void *buffer, size_t bytes_expected);
void read_num_bytes(int socket, void* buffer, size_t bytes_expected);

error_t server_parser(int key, char *arg, struct argp_state *state) {
	struct server_arguments *args = state->input;
	error_t ret = 0;
	switch(key) {
	case 'p':
		//zeroing out the struction	
		memset(&args->serv_addr, 0, sizeof(args->serv_addr)); // zero out the server address
		args->serv_addr.sin_family = AF_INET;
		args->serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //looking for any interface
		if(atoi(arg) == 0) {
			perror("invalid port\n");
		} else if(atoi(arg) <= 1024) {
			perror("port must be greater than 1024\n");
			exit(-1);
		} else {
			args->serv_addr.sin_port = htons(atoi(arg));
		}
		break;
	case 's':
		args->salt_len = strlen(arg);
		args->salt = malloc(args->salt_len);
		memcpy(args->salt, arg, args->salt_len);
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}


void server_parseopt(int argc, char *argv[]) {
	struct server_arguments args;

	/* bzero ensures that "default" parameters are all zeroed out */
	bzero(&args, sizeof(args));

	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{ "salt", 's', "salt", 0, "The salt to be used for the server. Zero by default", 0},
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got an error condition when parsing\n");
	}
	if(!(args.serv_addr.sin_port)) {
		perror("Port must be specified\n");
		exit(-1);
	}
	payload(args);
	free(args.salt);
}

void send_num_bytes(int socket, void *buffer, size_t bytes_expected) {
    size_t bytes_sent = 0;
    size_t result = 0;

    while(bytes_sent < bytes_expected){
        if((result = send(socket, buffer + bytes_sent, bytes_expected-bytes_sent, 0)) < 0) {
			perror("send() failed");
			exit(-1);
		}
        bytes_sent += result;
    }
}

void read_num_bytes(int socket, void* buffer, size_t bytes_expected) {
    size_t bytes_read = 0;
    size_t result;
    while (bytes_read < bytes_expected) {
        if((result = read(socket, buffer + bytes_read, bytes_expected - bytes_read)) < 0){
			perror("read() failed");
			exit(-1);
		}

        bytes_read += result;
    }
}

void payload(struct server_arguments args) {
	// Step 1: creating a TCP socket with socket()
	int serv_sock;
	int client_sock;

	if((serv_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP | SO_REUSEADDR)) < 0) {
		perror("socket() failed");
		exit(-1);
	}

	//making sure the address can be reused
	if((setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &serv_sock, sizeof(serv_sock))) < 0) {
		perror("setsockopt() failed");
		exit(-1);
	}


	//Step 2: assign port# with bind()
	if(bind(serv_sock, (struct sockaddr *)&args.serv_addr, sizeof(args.serv_addr)) < 0) {
		perror("bind failed");
		exit(-1);
	}

	//Step 3: allow connection via that port number
	if(listen(serv_sock, SOMAXCONN) < 0) {
		perror("listen() failed");
		exit(-1);
	}

	pid_t pid;
	socklen_t client_len; // length of client address data structure 
	struct sockaddr_in client_addr;
	unsigned int num_child = 0;

	for(;;) { // running forever
		client_len = sizeof(client_addr);
		if((client_sock = accept(serv_sock, (struct sockaddr *) &client_addr, &client_len)) < 0) {
			perror("accept() failed");
			exit(-1);
		}

		if((pid = fork()) < 0) {
			perror("fork() failed");
			exit(-1);
		} else if(pid == 0) { // child proccess 
			close(serv_sock); 

			for(;;) { // running forever
				// receiving initialization
				uint16_t op;
				uint32_t n;
				read_num_bytes(client_sock, &op, sizeof(op));
				read_num_bytes(client_sock, &n, sizeof(n));
				uint32_t num_hash = ntohl(n);

				// acknowledgment
				//[op	length]
				uint16_t op2 = htons((uint16_t) 2); 
				uint32_t length = htonl((uint32_t) (num_hash * 38));
				send_num_bytes(client_sock, &op2, sizeof(op2));
				send_num_bytes(client_sock, &length, sizeof(length));

				struct checksum_ctx *ctx;
				if(args.salt_len == 0){
                	ctx = checksum_create(NULL, 0);
				} else {
					ctx = checksum_create((uint8_t*)args.salt, args.salt_len);
				}

				uint8_t *data = malloc(UPDATE_PAYLOAD_SIZE);
				uint8_t *hashed_payload = malloc(32);

				for(int i = 0; i < num_hash; i ++) {
					uint16_t op3;
					uint32_t length, len;

					// recieving hash request
					read_num_bytes(client_sock, &op3, sizeof(op3));
					read_num_bytes(client_sock, &length, sizeof(length));
					len = ntohl(length);
					if(len <= 4096) {
						read_num_bytes(client_sock, data, len);
						checksum_finish(ctx, data, len, hashed_payload);
					} else {
						read_num_bytes(client_sock, data, UPDATE_PAYLOAD_SIZE);
						checksum_finish(ctx, data, UPDATE_PAYLOAD_SIZE, hashed_payload);
					}					
					checksum_reset(ctx);

					//sending hash response
					uint16_t op4 = htons((uint16_t) 4);
					uint32_t counter = htonl((uint32_t) i);
					send_num_bytes(client_sock, &op4, sizeof(op4));
					send_num_bytes(client_sock, &counter, sizeof(counter));
					send_num_bytes(client_sock, hashed_payload, 32);
				}
				checksum_destroy(ctx);
				free(data);
				free(hashed_payload);
			}
		}
		num_child++; /* Increment number of outstanding child processes */
		close(client_sock);
		
		//clean up the unnecessary childs
		while(num_child) {
			pid = waitpid((pid_t) -1, NULL, WNOHANG);  /* Non-blocking wait */
            if (pid < 0) {  
				perror("waitpid() failed");
                exit(-1);
			} else if (pid == 0) {  /* No child left to wait on */
                break;
			} else {
                num_child--;  
			}
		}
	}
}


int main(int argc, char **argv) {
    server_parseopt(argc, argv); 
	return 0;
}

