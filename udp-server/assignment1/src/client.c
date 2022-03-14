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
#include <fcntl.h>
#include <time.h>

struct client_arguments {
    struct sockaddr_in serv_addr;
	int time_request_num;
	int time_out;
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

typedef struct node{
	int seq_num;
	
	time_t client_sec;
	long client_nano;

	time_t server_sec;
	long server_nano;

	time_t current_sec;
	long current_nano;

	struct node *next;
} node;

void payload(struct client_arguments args);
void append(node **head, node *new_node);
void print_list(node* head);
void free_list(node *head);
int exist(node *list, int num);
node *return_node(node *list, int num);

error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;
	//int len;
	switch(key) {
    // IP address
	case 'a':
        //zeroing out the structure
        memset(&args->serv_addr, 0, sizeof(args->serv_addr)); 
        args->serv_addr.sin_family = AF_INET;
        args->serv_addr.sin_addr.s_addr = inet_addr(arg);
		if (!inet_pton(AF_INET, arg, &args->serv_addr.sin_addr.s_addr)) {
			argp_error(state, "Invalid address");
		} 
		break;
	case 'p':
        if(atoi(arg) <= 0) 
			argp_error(state, "Negative port");
        args->serv_addr.sin_port = htons(atoi(arg));
		break;
	case 'n':
		/* validate argument makes sense */
		args->time_request_num = atoi(arg);
		if(args->time_request_num < 0) 
			argp_error(state, "Negative time request");	
		break;
	case 't':
		args->time_out = atoi(arg);
		if(args->time_out < 0) 
			argp_error(state, "Negative time out");	
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

void client_parseopt(int argc, char *argv[]) {
	struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the server", 0},
		{ "time_request_num", 'n', "time_request_num", 0, "The number of time requests to send to the server", 0},
		{ "timeout", 't', "timeout", 0, "The number of time in seconds", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0 };
	struct client_arguments args;
	bzero(&args, sizeof(args));

    // after using argp_parse, args now have the user input
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		perror("Got error in parse\n");
		exit(-1);
	}
	if (!(args.serv_addr.sin_addr.s_addr)) {
		perror("IP address must be specified\n");
		exit(-1);
	}
	if (!(args.serv_addr.sin_port)) {
		perror("Port must be specified\n");
		exit(-1);
	}
	if(!args.time_request_num) {
		perror("time request must be specified\n");
		exit(-1);
	}
	if(!args.time_out) {
				perror("time out must be specified\n");
		exit(-1);
	}

    payload(args);
}

void payload(struct client_arguments args) {
    int sock;
    //creating a UDP connection
    if((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket() failed");
		exit(-1);
	}

	// constructing the time request and sending it
	time_request *time_req = (time_request *) malloc(sizeof(time_request));	
	time_response *time_res = (time_response *) malloc(sizeof(time_response));
	node *list = NULL;

	// sending
	for(int i = 0; i < args.time_request_num; i++) {
		memset(time_req, 0, sizeof(time_request));

		time_req->version = 1;
		time_req->seq_num = htobe16(i+1);

		struct timespec time;
		memset(&time, 0, sizeof(time));

		if(clock_gettime(CLOCK_REALTIME, &time) < 0) {
			perror("clock_gettime() failed");
			exit(-1);
		}
		time_t sec = time.tv_sec;
		long nano = time.tv_nsec;
		time_req->seconds = htobe64((uint64_t) sec);
		time_req->nano_secs = htobe64((uint64_t) nano);

		if((sendto(sock, time_req, sizeof(*time_req), 0, 
			(struct sockaddr *) &args.serv_addr, sizeof(args.serv_addr))) <0) {
			perror("sendto() failed");
			exit(-1);
		} 
	} 

	// receiving the time response
	for(int i = 0; i < args.time_request_num; i ++) {
		int recv_msg_size = 0;
		struct sockaddr_in serv_addr;
		socklen_t serv_len = sizeof(serv_addr); 
		memset(time_res, 0, sizeof(time_response));

		//setting up timeout request
		struct timeval tv;
		tv.tv_sec = args.time_out;
		tv.tv_usec = 0;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
			perror("setsockopt() failed");
			exit(-1);
		}

		if((recv_msg_size = 
			recvfrom(sock, time_res, sizeof(time_response), 0, (struct sockaddr*) &serv_addr, &serv_len)) < 0) {
				break;
		}
		if(recv_msg_size > 0) {
			//succesfull packet
			struct timespec curr_time;
			memset(&curr_time, 0, sizeof(curr_time));

			if(clock_gettime(CLOCK_REALTIME, &curr_time) < 0) {
				perror("clock_gettime() failed");
				exit(-1);
			}

			node *new_node = (node *) malloc(sizeof(node));
			new_node->seq_num = be16toh(time_res->seq_num);

			new_node->client_sec = be64toh(time_res->seconds);
			new_node->client_nano = be64toh(time_res->nano_secs);

			new_node->server_sec = be64toh(time_res->serv_seconds);
			new_node->server_nano = be64toh(time_res->serv_nano_secs);

			new_node->current_sec = curr_time.tv_sec;
			new_node->current_nano = curr_time.tv_nsec;

			new_node->next = NULL;
			append(&list, new_node);
		}
	}

	// printing out the result
	for(int i = 1; i <= args.time_request_num; i ++) {
		if(exist(list, i) == 0) {
			printf("%d: Dropped\n", i);
		} else {
			node *temp = return_node(list, i);
			
			time_t T0_sec = temp->client_sec, //client second
					T1_sec = temp->server_sec, //server second
					T2_sec = temp->current_sec; //current second
			
			time_t T0_nano = temp->client_nano, //client nano
					T1_nano = temp->server_nano, //server nano
					T2_nano = temp->current_nano; //current nano

			time_t sec = T1_sec - T0_sec + T1_sec - T2_sec;
			long nano = T1_nano - T0_nano + T1_nano - T2_nano;		

			double theta = (double)sec / 2.0 + (double)nano / 2000000000.0;
			double delta = (double)(T2_sec - T0_sec) + (double)(T2_nano - T0_nano) / 1000000000.0; 
			printf("%d: %.4f %.4f\n", i, theta, delta);
		}
	}

	free_list(list);
	free(time_req);
	free(time_res);
	close(sock);
	exit(0);
}

//append new node into linked list accoridng to sequence number
void append(node** head_ref, node* new_node) { 
	node* current; 
	/* Special case for the head end */
	if (*head_ref == NULL || (*head_ref)->seq_num > new_node->seq_num) { 
		new_node->next = *head_ref; 
		*head_ref = new_node; 
	} else { 
		current = *head_ref; 
		while (current->next != NULL 
			&& current->next->seq_num <= new_node->seq_num) { 
			current = current->next; 
		} 
		if(new_node->seq_num != current->seq_num) {
    		new_node->next = current->next; 
    		current->next = new_node; 
		}
	} 
} 

void free_list(node *head) {
	node *current;
	while(head != NULL) {
		current = head;
		head = head->next;
		free(current);
	}
}

// returns 1 if target exist in list, 0 other wise
int exist(node *list, int num) {	
	node *current = list;
	while(current != NULL) {
		if(current->seq_num == num) {
			return 1;
		}
		current = current->next;
	}
	return 0;
}

node *return_node(node *list, int num) {
	node *current = list;
	while(current != NULL) {
		if(current->seq_num == num) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

int main(int argc, char **argv) {
    client_parseopt(argc, argv); 
    return 0;
}
