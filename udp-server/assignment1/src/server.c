#include <argp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> 
#include <netdb.h>
#include <endian.h>
#include <time.h>
#include <math.h>

struct server_arguments {
	struct sockaddr_in serv_addr;
	int percent;
};

typedef struct time_request {
	uint8_t version;
	uint16_t seq_num;
	uint64_t seconds;
	uint64_t nano_secs;
} time_request;

typedef struct time_response {
	uint8_t version;
	uint16_t seq_num;
	uint64_t seconds;
	uint64_t nano_secs;

	uint64_t serv_seconds;
	uint64_t serv_nano_secs;
} time_response;

typedef struct node {
	int seq_num;
	char addr[1025];
    char port[1025];
	time_t last_update;
	struct node *next;
} node;

void payload(struct server_arguments args);
void update_list(node **head, char *addr, char *port, int new_seq, time_t curr_time);
node *create_node(char *addr, char *port, int new_seq, time_t curr_time);

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
			argp_error(state, "invalid port\n");
		} else if(atoi(arg) <= 1024) {
			argp_error(state, "port must be greater than 1024");
		} else {
			args->serv_addr.sin_port = htons(atoi(arg));
		}
		break;
	case 'd':
		args->percent = atoi(arg);
		if(args->percent < 0 || args->percent > 100) {
			argp_error(state, "port must be greater than 1024");
		}
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
		{ "percent", 'd', "percent", 0, "The percent to be used for the server. Zero by default", 0},
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
	if(!args.percent) {
		args.percent = 0;
	}
	payload(args);
}


void payload(struct server_arguments args) {
	// Step 1: creating a TCP socket with socket()
	int serv_sock;

	if((serv_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
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

	srand(time(NULL));
	socklen_t client_len; // length of client address data structure 
	struct sockaddr_in client_addr;
	int recv_msg_size;
	time_request *time_req = malloc(sizeof(time_request));
	time_response *time_res = malloc(sizeof(time_response));
	node *list = NULL;

	for(;;) { // running forever
		client_len = sizeof(client_addr);

		if((recv_msg_size = 
			recvfrom(serv_sock, time_req, sizeof(time_request), 0, (struct sockaddr*) &client_addr, &client_len)) < 0) {
				perror("recvfrom() failed");
				exit(-1);
		}
		int random = ((rand()%(100+1)+0));
		if(args.percent == 0 || random > args.percent) {
			// getting the info of the client
			socklen_t clntLen = sizeof(struct sockaddr_storage);
            char clntAddr[NI_MAXHOST] = {0};
            char clntPort[NI_MAXSERV] = {0};
            getnameinfo((struct sockaddr *)&client_addr, clntLen, clntAddr, 
				sizeof(clntAddr), clntPort, sizeof(clntPort), 
				NI_NUMERICHOST | NI_NUMERICSERV);
			
			uint16_t client_seq = be16toh(time_req->seq_num);
			
			// getting the server time
			memset(time_res, 0, sizeof(time_response));
			struct timespec time;
			memset(&time, 0, sizeof(time));

			if(clock_gettime(CLOCK_REALTIME, &time) < 0) {
				perror("clock_gettime() failed");
				exit(-1);
			}
			time_t sec = time.tv_sec;
			long nano = time.tv_nsec;

			//updating the linked list
			update_list(&list, clntAddr, clntPort, client_seq, sec);

			// setting up the time request
			time_res->version = time_req->version;
			time_res->seq_num = time_req->seq_num;
			time_res->seconds = time_req->seconds;
			time_res->nano_secs = time_req->nano_secs;
			time_res->serv_seconds = htobe64((uint64_t) sec);
			time_res->serv_nano_secs = htobe64((uint64_t) nano);

			if((sendto(serv_sock, time_res, sizeof(time_response), 0, 
				(struct sockaddr *) &client_addr, client_len)) < 0) {
				perror("sendto() failed");
				exit(-1);
			} 
		} //else printf("dropped %d with random: %d\n", be16toh(time_req->seq_num), random);
	}
	free(time_req);
	exit(0);
}

int main(int argc, char **argv) {
    server_parseopt(argc, argv); 
	return 0;
}

void update_list(node **head, char *addr, char *port, int new_seq, time_t curr_time) {
	// adding to an empty list
    if(*head == NULL) {
        node *new_node = create_node(addr, port, new_seq, curr_time);
		*head = new_node;
        return;
    }
    node *current = *head;
    node *prev = NULL;
    int bool = 0;

    while(current != NULL) {   
        if(!strcmp(current->addr, addr) && !strcmp(current->port, port)) {
            bool = 1;
			// 2 minutes timeout
            if(difftime(curr_time, current->last_update) >= 120) {
                current->seq_num = new_seq;
                current->last_update = curr_time;
            } else {
                if(current->seq_num > new_seq) {
                    printf("%s:%s %d %d\n", current->addr, current->port, new_seq, current->seq_num);
                } else {
                    current->seq_num = new_seq;
                    current->last_update = curr_time;
                }
            }
        } else {
            if(difftime(curr_time, current->last_update) >= 120) {
                current->seq_num = 0;
                current->last_update = curr_time;
            }
        }
        prev = current;
        current = current->next;
    }
	
	// adding to the end of the list
    if(!bool) {
        node *new_node = create_node(addr, port, new_seq, curr_time);
        prev->next = new_node;
    }
}

node *create_node(char *addr, char *port, int new_seq, time_t curr_time) {
	node *new_node = (node *)malloc(sizeof(node));
    new_node->seq_num = new_seq;
    new_node->next = NULL;
    new_node->last_update = curr_time;
	strncpy(new_node->addr, addr, strlen(addr)+1);
    strncpy(new_node->port, port, strlen(port)+1); 
	return new_node;
}

