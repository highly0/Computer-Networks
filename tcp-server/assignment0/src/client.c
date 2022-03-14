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

struct client_arguments {
    struct sockaddr_in serv_addr;
	int hashnum;
	int smin;
	int smax;
	int filename;
	struct stat fstat;
};


typedef struct init_payload {
	uint16_t op;
	uint32_t n;
} init_payload;


void payload(struct client_arguments args);
void send_num_bytes(int socket, void *buffer, size_t bytes_expected);
void read_num_bytes(int socket, void* buffer, size_t bytes_expected);


error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;
	//int len;
	switch(key) {
    // IP address
	case 'a':
        //zeroing out the structure
        memset(&args->serv_addr, 0, sizeof(args->serv_addr)); // Zero out structures
        args->serv_addr.sin_family = AF_INET;
        args->serv_addr.sin_addr.s_addr = inet_addr(arg);
		if (!inet_pton(AF_INET, arg, &args->serv_addr.sin_addr.s_addr)) {
			argp_error(state, "Invalid address");
		} 
		break;
	case 'p':
        if(atoi(arg) < 0) argp_error(state, "Negative port");
        args->serv_addr.sin_port = htons(atoi(arg));
		break;
	case 'n':
		/* validate argument makes sense */
		args->hashnum = atoi(arg);
		if(args->hashnum < 0) {
			perror("hashnum must be a number >= 0\n");
			exit(-1);
		} 		
		break;
	case 300:
		/* validate arg */
		args->smin = atoi(arg);
		if (args->smin <= 0) {
			perror("smin must be a number >= 1\n");
			exit(-1);
		} 
		break;
	case 301:
		/* validate arg */
		args->smax = atoi(arg);
		if (args->smax < args->smin) {
			perror("smax must be a number >= smin\n");
			exit(-1);
		} else if(args->smax > 16777216) {
			perror("smax must be a number <= 2^24\n");
			exit(-1);
		} 
		break;
	case 'f':
		/* validate and open the file */
		args->filename = open(arg, O_RDONLY);
		if (args->filename == -1) {
			perror("open() failed\n");
			exit(-1);
		} else if ((fstat(args->filename, &args->fstat)) < 0) {
			perror("fstat() failed\n");
			exit(-1);
		}
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
		{ "hashreq", 'n', "hashreq", 0, "The number of hash requests to send to the server", 0},
		{ "smin", 300, "minsize", 0, "The minimum size for the data payload in each hash request", 0},
		{ "smax", 301, "maxsize", 0, "The maximum size for the data payload in each hash request", 0},
		{ "file", 'f', "file", 0, "The file that the client reads data from for all hash requests", 0},
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
	if(!args.hashnum) {
		perror("hashnum must be specified\n");
		exit(-1);
	}
	if(!args.smin) {
		perror("smin must be specified\n");
		exit(-1);
	}
	if(!args.smax) {
		perror("smax must be specified\n");
		exit(-1);
	}
	if (!(args.filename)) {
		perror("File must be specified\n");
		exit(-1);
	}
	if (args.fstat.st_size < (args.hashnum * args.smax)) {
		perror("File is too small\n");
		exit(-1);
	}
    payload(args);
}

void send_num_bytes(int socket, void *buffer, size_t bytes_expected) {
    size_t bytes_sent = 0;
    size_t result = 0;

    while(bytes_sent < bytes_expected){
        if((result = send(socket, buffer + bytes_sent, bytes_expected - bytes_sent, 0)) < 0) {
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

void payload(struct client_arguments args) {
	srand(time(NULL));
    // Step 1: Create a TCP socket via socket()
    int sock;
    //creating a TCP connection
    if((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP | SO_REUSEADDR)) < 0) {
		perror("socket() failed\n");
		exit(-1);
	}
    
    // Step 2: Establish connection to server via connect()
    if(connect(sock, (struct sockaddr *) &args.serv_addr, sizeof(args.serv_addr)) < 0) {
		perror("connect() failed");
		exit(-1);
	}

	//initialization
	//[op	n]
	uint16_t op = htons((uint16_t) 1); 
	uint32_t n = htonl((uint32_t) args.hashnum);
	send_num_bytes(sock, &op, sizeof(op));
	send_num_bytes(sock, &n, sizeof(n));

	//reading the ack
	uint16_t op2;
	uint32_t length;
	read_num_bytes(sock, &op2, sizeof(op2));
	read_num_bytes(sock, &length, sizeof(length));

	uint8_t *data = malloc(args.smax);
	uint8_t *hashed_payload = malloc(32);

	for(int i = 0; i < args.hashnum; i ++) {
		// finding a random L between smax and smin
		int L, upper = args.smax, lower = args.smin;
		L = (rand() % (upper - lower + 1) + lower);	
		read(args.filename, data, L);

		// sending the hash request
		uint16_t op3 = htons((uint16_t) 3);
		uint32_t payload_length = htonl((uint32_t) L);
		send_num_bytes(sock, &op3, sizeof(op3)); 
		send_num_bytes(sock, &payload_length, sizeof(payload_length));
		send_num_bytes(sock, data, L);

		// receiving the hash
		uint16_t op4;
		uint32_t counter;
		read_num_bytes(sock, &op4, sizeof(op4));
		read_num_bytes(sock, &counter, sizeof(counter));
		read_num_bytes(sock, hashed_payload, 32);


		printf("%u: 0x", ntohl(counter));		
		for (int i = 0; i < 32; ++i) {
			printf("%02x", hashed_payload[i]);
		}  
		printf("\n"); 
	}
	close(args.filename);
	close(sock);
	free(data);
	free(hashed_payload);
	exit(0);
}

int main(int argc, char **argv) {
    client_parseopt(argc, argv); 
    return 0;
}