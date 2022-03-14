#include "rserver.h" 

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
			perror("invalid port");
			exit(-1);
		} else if(atoi(arg) <= 1024) {
			perror("port must be greater than 1024");
			exit(-1);
		} else {
			args->serv_addr.sin_port = htons(atoi(arg));
		}
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

int array[30];

void server_parseopt(int argc, char *argv[]) {
	struct server_arguments args;
	/* bzero ensures that "default" parameters are all zeroed out */
	bzero(&args, sizeof(args));

	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got an error condition when parsing\n");
	}
	if(!(args.serv_addr.sin_port)) {
		perror("Port must be specified");
		exit(-1);
	}
	TCP_connect(args);
}

void TCP_connect(struct server_arguments args) {
	int serv_sock;
    int max_sd = 0, activity;

    //set of socket descriptors  
    fd_set readfds;   
	init_arrays();

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

	for(;;) { // running forever
        //clear the socket set  
        FD_ZERO(&readfds);  

        //add master socket to set  
        FD_SET(serv_sock, &readfds);   
        max_sd = serv_sock;    

        //add child sockets to set  
        for(int i = 0 ; i < MAX_CLIENTS; i++) {   
            //if valid socket descriptor then add to read list  
            if(client_socket[i]->sockfd > 0) 
                FD_SET(client_socket[i]->sockfd, &readfds);  
                 
            //highest file descriptor number, need it for the select function  
            if(client_socket[i]->sockfd > max_sd)   
                max_sd = client_socket[i]->sockfd;   
        }   

        if((activity = select(max_sd + 1 , &readfds , NULL , NULL , NULL)) < 0
            && (errno!=EINTR)) {
            perror("select() failed");
            exit(-1);
        }

		//new connection
		if(FD_ISSET(serv_sock, &readfds)) {	
			handle_incoming_client(serv_sock);
		}
		
		// existing connection
		for(int i = 0; i < MAX_CLIENTS; i ++) {
			if(FD_ISSET(client_socket[i]->sockfd, &readfds)) {
				handle_existing_client(client_socket[i]);
			}
		}
	}
}

int counter = 0; // for all of the rand nick names

// handling new connection
void handle_incoming_client(int server_sock) {
	int client_sock, leave_flag = 0;
	socklen_t client_len; // length of client address data structure 
	struct sockaddr_in client_addr;
	client_len = sizeof(client_addr);

	if((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_len)) < 0) {
		perror("accept() failed");
		exit(-1);
	} 

	printf("New connection , socket fd is %d , ip is : %s , port : %d\n", 
	client_sock , inet_ntoa(client_addr.sin_addr) , ntohs(client_addr.sin_port)); 

	// hand shake
	uint8_t *hello_payload = (uint8_t *) malloc(12); 
	read_num_bytes(client_sock, hello_payload, 12, &leave_flag);
	free(hello_payload);	

	// setting the nick names for the user
	char *nick = malloc(6);
	sprintf(nick, "rand");
	sprintf(nick + 4, "%d", counter++);
	nick[5] = '\0'; // NULL terminated

	size_t payload_size = 0;
	uint8_t *payload = create_server_res(0, (uint8_t *)nick, 5, &payload_size, 0);
	send_num_bytes(client_sock, payload, payload_size);

	// TODO only add if sockfd is 0 and the next sock fd is 0 -> FINISH
	//add new socket to array of sockets  
	for(int i = 0; i < MAX_CLIENTS; i++) {   
		if(i < MAX_CLIENTS - 1) {
			if(client_socket[i]->sockfd == 0 && client_socket[i+1]->sockfd == 0) {  
				client_socket[i]->sockfd = client_sock; 
				client_socket[i]->address = client_addr;
				client_socket[i]->user_name = (uint8_t *)nick;
				client_socket[i]->room = rooms[0]; // main room

				// adding all new clients to the main room
				rooms[0]->client_array[i]->sockfd = client_sock; 
				rooms[0]->client_array[i]->address = client_addr;
				rooms[0]->client_array[i]->user_name = (uint8_t *)nick;
				break;   
			}  
		} else {
			printf("Max number of clients\n");
			exit(-1);
		} 
	} 
} 

void handle_existing_client(client_t *client) {
	int leave_flag = 0;

	uint8_t *header = (uint8_t *) malloc(7);
	read_num_bytes(client->sockfd, header, 7, &leave_flag);

	if(leave_flag == 1) {
		printf("client is closing\n");
		if(counter > 0) // decrement counter for the next client
			counter --;
		// removing the client from both the client_sock array and the room's client array
		room_user_remove(client->room, client->user_name, strlen((char *) client->user_name));
		client_sock_remove(client->user_name, strlen((char *) client->user_name));
	}

	size_t length = ntohl(*(uint32_t *)&header[2]);
	uint8_t flag = header[6];
	
	handle_command(client, flag, length);
	free(header);
}

// handles all of the client's command
void handle_command(client_t *client, uint8_t flag, size_t payload_len) {
	int leave_flag = 0;

	// changing nickname TODO: Name must be < 256
	if(flag == 0x1b) {
		uint8_t *data = malloc(payload_len + 1);
		read_num_bytes(client->sockfd, data, payload_len, &leave_flag); 
		data[payload_len] = '\0'; // NULL terminated
		data++; // first index is blank?

		if(payload_len > 255) {
			uint8_t *msg = (uint8_t *) "Nick is longer than 255 characters.";
			send_failure_payload(client->sockfd, msg);
		} else {
			if(user_exist(client->room, data, payload_len) == 1) {
				uint8_t *msg = (uint8_t *) "Someone already nicked that nick.";
				send_failure_payload(client->sockfd, msg);
			} else {
				if(counter > 0)  // decrement counter for the next client
					counter --;

				// updating the nick in the room's client array
				change_nick_in_room(client->room, client->user_name, strlen((char *)client->user_name), data, payload_len);
				// updating the nick in the clients array
				client->user_name = data;
				send_sucess_payload(client->sockfd);
			}
		}
		//free(data);
	} else if(flag == 0x1a) { // send a list of users
		int len = 0;
		for(int i = 0; i < MAX_CLIENTS; i ++) {
			if(client->room->client_array[i]->sockfd > 0) {
				len += 1 + strlen((char *)client->room->client_array[i]->user_name);
			}
		}
		char *users = malloc(len);

		int idx = 0;
		for(int i = MAX_CLIENTS-1; i >=0; i --) {
			if(client->room->client_array[i]->sockfd > 0) {
				int name_len = strlen((char *)client->room->client_array[i]->user_name);
				users[idx] = name_len;
				memcpy(users + idx + 1, (char *)client->room->client_array[i]->user_name, name_len); 
				idx += 1 + name_len;
			}
		} 
		size_t payload_size = 0;
		uint8_t *payload =create_server_res(0, (uint8_t*) users, len, &payload_size, 1);
		send_num_bytes(client->sockfd, payload, payload_size);
		//free(payload);
	} else if(flag == 0x1c) { // send a message to a certain user
		uint8_t user_len; 
		read_num_bytes(client->sockfd, &user_len, sizeof(uint8_t), &leave_flag); 

		uint8_t *user = malloc(user_len + 1);
		read_num_bytes(client->sockfd, user, user_len, &leave_flag); 
		
		uint8_t null_byte;
		read_num_bytes(client->sockfd, &null_byte, sizeof(uint8_t), &leave_flag); 
 
		uint8_t msg_len; 
		read_num_bytes(client->sockfd, &msg_len, sizeof(uint8_t), &leave_flag); 

		if(msg_len >= MAX_COMMAND_LEN) {
			uint8_t *error = (uint8_t *) "Uh oh, message too large.";
			send_failure_payload(client->sockfd, error);
		} else {
			uint8_t *msg = malloc(msg_len);
			read_num_bytes(client->sockfd, msg, msg_len, &leave_flag);

			if(user_exist(rooms[0], user, (size_t) user_len) == 1) {
				// send the message to that user
				send_message(client->room, msg, msg_len, user, (size_t) user_len, client->user_name, strlen((char*) client->user_name));		
				send_sucess_payload(client->sockfd);
			} else {
				uint8_t *msg = (uint8_t *) "Nick not found.";
				send_failure_payload(client->sockfd, msg);
			}
		}
		//free(user);
		//free(msg);
	} else if(flag == 0x17) { // create a room
		uint8_t room_len; 
		read_num_bytes(client->sockfd, &room_len, sizeof(uint8_t), &leave_flag);
	
		uint8_t *room_name = malloc(room_len + 1);
		read_num_bytes(client->sockfd, room_name, room_len, &leave_flag);
		room_name[room_len] = '\0';

		uint8_t password_len;
		read_num_bytes(client->sockfd, &password_len, sizeof(uint8_t), &leave_flag);

		uint8_t *password;
		if(password_len == 0x00) {
			// no password
			password = NULL;
		} else {
			// yes password
			password = malloc(password_len);
			read_num_bytes(client->sockfd, password, password_len, &leave_flag);
		}

		if(room_len > 255) {
			uint8_t *msg = (uint8_t *) "Room name is longer than 255 characters.";
			send_failure_payload(client->sockfd, msg);
		} else if(password_len >= 255) {
			uint8_t *msg = (uint8_t *) "Room password is longer than 255 characters.";
			send_failure_payload(client->sockfd, msg);
		} else {
			// if the user tries to join a room while already in a room
			if(memcmp((uint8_t *)"main_room", client->room->room_name, strlen((char *) "main_room")) != 0) {
				printf("room: %s\n", (char *) client->room->room_name);
				uint8_t *msg = (uint8_t *) "You fail to bend space and time to reenter where you already are.";
				send_failure_payload(client->sockfd, msg);
			} else {
				int room_idx = room_exist(room_name, room_len);

				// if room already exists
				if(room_idx > 0) {
					client->room = rooms[room_idx];
					if(room_user_add(rooms[room_idx], password, password_len, client) == 1) {
						send_sucess_payload(client->sockfd);
					} else {
						uint8_t *msg = (uint8_t *) "Wrong password. You shall not pass!";
						send_failure_payload(client->sockfd, msg);
						client->room = rooms[0]; // return back to main room
					}
				} else { // if room does not exist
					int index;
					create_room(room_name, password, &index);
					client->room = rooms[index];
					room_user_add(client->room, password, password_len, client);
					send_sucess_payload(client->sockfd);
				}
			}
		}
	} else if(flag == 0x18) { // leave a room
		int leave = 1;
		for(int i = 0; i < MAX_CLIENTS; i++) {
			uint8_t *name = client->room->client_array[i]->user_name;
			if(name != NULL) {
				if(memcmp(name, client->user_name, strlen((char*)name)) != 0) {
					// if there are still people in the room, don't leave
					leave = 0;
					break;
				}
			}	
		}
		uint8_t *room = client->room->room_name;
		room_user_remove(client->room, client->user_name, strlen((char *) client->user_name));
		client->room = rooms[0]; // return to the main room
		if(leave == 1) 
			remove_room(room, strlen((char *) room));
		send_sucess_payload(client->sockfd);
	} else if(flag == 0x19) { // list rooms
		int len = 0;
		for(int i = 1; i < MAX_ROOMS; i ++) {
			if(rooms[i]->room_name != NULL) {
				len += 1 + strlen((char *)rooms[i]->room_name);
			}
		}
		char *room_list = malloc(len);

		int idx = 0;
		for(int i = MAX_ROOMS-1; i >= 1; i--) {
			if(rooms[i]->room_name != NULL) {
				int name_len = strlen((char *)rooms[i]->room_name);
				room_list[idx] = name_len;
				memcpy(room_list + idx + 1, (char *)rooms[i]->room_name, name_len); 
				idx += 1 + name_len;
			}
		} 
		size_t payload_size = 0;
		uint8_t *payload =create_server_res(0, (uint8_t*) room_list, len, &payload_size, 1);
		send_num_bytes(client->sockfd, payload, payload_size);
	} else if(flag == 0x1d){
		int flag = 0;
		uint8_t *trash = malloc(payload_len);
		read_num_bytes(client->sockfd, trash, payload_len, &flag);

		uint8_t *msg = (uint8_t *) "You shout into the void and hear nothing but silence.";
		send_failure_payload(client->sockfd, msg);
	} 
} 

// [04 17 00 00 00 message_length fe code message]
uint8_t *create_server_res(int code, uint8_t *message, size_t message_len, size_t *payload_size, int add_null) {
	uint8_t *response = (uint8_t *) malloc(7 + message_len + 1);
	*(uint16_t *)response = htons(0x0417);
	*(uint32_t *)&response[2] = htonl(1 + message_len);
	response[6] = 0xfe;
	if(add_null == 1) {
		response[7] = 0x00;
		//response[8] = code;
		memcpy(response + 8, message, message_len);
		*payload_size = 8 + message_len;
	} else {
		response[7] = code;
		memcpy(response + 8, message, message_len);
		*payload_size = 8 + message_len;
	}
	return response;
}

void send_sucess_payload(int sock) {
	size_t payload_size = 0;
	uint8_t *payload = create_server_res(0, NULL, 0, &payload_size, 0);
	send_num_bytes(sock, payload, payload_size);
}

void send_failure_payload(int sock, uint8_t *msg) {
	size_t payload_size = 0;
	uint8_t *payload = create_server_res(strlen((char *) msg), msg, strlen((char *)msg), &payload_size, 0);
	send_num_bytes(sock, payload, payload_size);
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

// read in a number of bytes, change flag to 1 if client disconnect
void read_num_bytes(int socket, void* buffer, size_t bytes_expected, int *flag) {
    size_t bytes_read = 0;
    size_t result;
    while (bytes_read < bytes_expected) {
		result = read(socket, buffer + bytes_read, bytes_expected - bytes_read);
        if(result < 0){
			perror("read() failed");
			exit(-1);
		} else if(result == 0) {
			*flag = 1;
			break;
		} 
        bytes_read += result;
    }
}

// sent from person a to person b
/* Send message to the specified client in a specic environment*/
void send_message(room_t *room, uint8_t *data, size_t data_len, uint8_t *to, size_t to_len, uint8_t *from, size_t from_len) {
	//client_t **clients = room->client_array;
	client_t **clients = client_socket;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(clients[i]->sockfd > 0) {
			if(memcmp(to, clients[i]->user_name, to_len) == 0) {
				printf("from: %s, sending to: %s\n", (char*) from, (char *)to);
				printf("to user has socket fd %d , ip %s , port %d\n", 
				clients[i]->sockfd , inet_ntoa(clients[i]->address.sin_addr) , ntohs(clients[i]->address.sin_port)); 

				// crafting a payload to the from user
				// total len 	user_len	user		NULL	size	data	
				int msg_len =    1+	 		from_len+ 	1+ 		1+		data_len;
				uint8_t *header = (uint8_t *) malloc(7 + msg_len);
				*(uint16_t *)header = htons(0x0417);
				*(uint32_t *)&header[2] = htonl(msg_len);
				header[6] = 0x1c;

				header[7] = from_len;
				memcpy(header + 7 + 1, from, from_len);
				*(uint16_t *)&header[7 + 1 + from_len] = htons(data_len);
				memcpy(header + 7 + 1 + from_len + 2, data, data_len);
				send_num_bytes(clients[i]->sockfd, header, 7 + msg_len);
				break;
			} 
		}
	}
}

int main(int argc, char **argv) {
    server_parseopt(argc, argv); 
	return 0;
}