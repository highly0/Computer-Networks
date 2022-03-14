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
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#define MAX_COMMAND_LEN 65536
#define MAX_CLIENTS 30
#define MAX_ROOMS 30

struct server_arguments {
	struct sockaddr_in serv_addr;
};

/* Client structure */
typedef struct client_t {
	struct sockaddr_in address;
	int sockfd;
	uint8_t *user_name;
	struct room_t *room;
} client_t;

// Room structure 
typedef struct room_t {
	uint8_t *room_name;
	uint8_t *password;
	client_t *client_array[MAX_CLIENTS];
} room_t;

void init_arrays();
void TCP_connect(struct server_arguments args);
void send_num_bytes(int socket, void *buffer, size_t bytes_expected);
void read_num_bytes(int socket, void* buffer, size_t bytes_expected, int *flag);

void client_sock_remove(uint8_t *user, size_t len);
void handle_command(client_t *client, uint8_t flag, size_t payload_len);
void handle_incoming_client(int server_sock);
void handle_existing_client(client_t *client);
int user_exist(room_t *room, uint8_t *user, size_t len);

void create_room(uint8_t *room, uint8_t *password, int *index);
void remove_room(uint8_t *room, size_t room_len);
void room_user_remove(room_t *room, uint8_t *user, size_t len);
int room_user_add(room_t *room, uint8_t *password, size_t password_len, client_t *client);
int room_exist(uint8_t *room_name, size_t len);
void change_nick_in_room(room_t *room, uint8_t *old_user, size_t old_user_len, uint8_t *new_user, size_t new_user_len);

uint8_t *create_server_res(int code, uint8_t *message, size_t message_len, size_t *payload_size, int add_null);
void send_message(room_t *room, uint8_t *data, size_t data_len, uint8_t *to, size_t to_len, uint8_t *from, size_t from_len);
void send_sucess_payload(int sock);
void send_failure_payload(int sock, uint8_t *msg);

// for debugging
void print_client();
void print_main_room_client();
void print_room_client(room_t *room);

client_t *client_socket[MAX_CLIENTS] = {NULL};
room_t *rooms[MAX_ROOMS] = {NULL};


void print_client() {
	for(int i = 0; i < MAX_CLIENTS; i ++) {
		if(client_socket[i]->sockfd > 0) {
			printf("%s  ", (char *)client_socket[i]->user_name);
		}
	} 
	printf("\n");
}

void print_main_room_client() {
	for(int i = 0; i < MAX_CLIENTS; i ++) {
		if(rooms[0]->client_array[i]->sockfd > 0) {
			printf("%s  ", (char *)rooms[0]->client_array[i]->user_name);
		}
	} 
	printf("\n");
}

void print_room_client(room_t *room) {
	//printf("in print_room\n");
	if(room->client_array == NULL) {
		printf("list is NULL\n");
		return;
	}
	client_t **clients = room->client_array;
	for(int i = 0; i < MAX_CLIENTS; i ++) {
		if(clients[i]->sockfd > 0 && clients[i]->user_name != NULL) {
			//printf("in if statement\n");
			printf("%s  ", (char *)clients[i]->user_name);
		}
	} 
	printf("\n");
}

void init_arrays() {
	// initialize the clients array
    for(int i = 0; i < MAX_CLIENTS; i++) { 
		client_t *new_client = (client_t *) malloc(sizeof(client_t)); 
		new_client->sockfd = 0;
		new_client->user_name =(uint8_t *) NULL; 
		new_client->room = NULL;
        client_socket[i] = new_client;   
    } 
	
	// initialize the main room
	room_t *main_room = (room_t *) malloc(sizeof(room_t));
	main_room->room_name = (uint8_t *)"main_room";
	for(int i = 0; i < MAX_CLIENTS; i++) { 
		client_t *new_client = (client_t *) malloc(sizeof(client_t)); 
		new_client->sockfd = 0;
		new_client->user_name =(uint8_t *) NULL; 
		main_room->client_array[i] = new_client;   
    } 
	main_room->password = NULL;
	rooms[0] = main_room;

	// initialize all of the other rooms
	for(int i = 1; i < MAX_ROOMS; i ++) {
		room_t *new_room = (room_t *) malloc(sizeof(room_t));
		new_room->room_name = NULL;
		new_room->password = NULL;
		
		// initialize the room's client array
		for(int j = 0; j < MAX_CLIENTS; j++) { 
			client_t *new_client = (client_t *) malloc(sizeof(client_t)); 
			new_client->sockfd = 0;
			new_client->user_name =(uint8_t *) NULL; 
			new_room->client_array[j] = new_client;   
    	} 
		rooms[i] = new_room;
	}
}

// returns 1 if room exist, 0 otherwise 
int room_exist(uint8_t *room_name, size_t len) {
	// starts from 1 because room[0] is main_room
	for(int i = 1; i < MAX_ROOMS; i ++) {
		if(rooms[i]->room_name != NULL) {
			if(memcmp(room_name, rooms[i]->room_name, len) == 0) {
				return 1;
			} 
		}
	}
	return 0;
}

// return index if exist in specified room, 0 otherwise
int user_exist(room_t *room, uint8_t *user, size_t len) {
	client_t **clients = room->client_array;
	for(int i = 0; i < MAX_CLIENTS; i ++) {
		if(clients[i]->sockfd > 0) {	
			if(memcmp(user, clients[i]->user_name, len) == 0) {
				return 1;
			} 
		}
	}
	return 0;	
}

// TODO: only add if rooms[i] name AND rooms[i+1] name are both NULL -> FINISH
// create a room and return the index of the room
void create_room(uint8_t *room, uint8_t *password, int *index) {
	// starts from 1 because room[0] is main_room
	for(int i = 1; i < MAX_ROOMS; i ++) {
		if(i < MAX_ROOMS - 1) {
			if(rooms[i]->room_name == NULL && rooms[i+1]->room_name == NULL) {
				rooms[i]->room_name = room;
				rooms[i]->password = password;
				*index = i;
				break;
			}
		} else {
			printf("Max number of rooms\n");
			exit(-1);
		}
	}
}

// remove a room in the room array
void remove_room(uint8_t *room, size_t room_len) {
	for(int i = 1; i < MAX_ROOMS; i ++) {
		if(rooms[i]->room_name != NULL) {
			if(memcmp(room, rooms[i]->room_name, room_len) == 0) {
				rooms[i]->room_name = NULL;
				rooms[i]->password = NULL;
				//rooms[i]->client_array = NULL;
				break;
			}
		}
	}
}

/* Remove clients from the client array */
void client_sock_remove(uint8_t *user, size_t len) {
	for(int i = 0; i < MAX_CLIENTS; i ++) {
		if(client_socket[i]->sockfd > 0) {
			if(memcmp(user, client_socket[i]->user_name, len) == 0) {
				//free(client_socket[i]->user_name);
				close(client_socket[i]->sockfd);
				client_socket[i]->sockfd = 0;
				client_socket[i]->room = NULL;
				//free(client_socket[i]);
				break;
			} 
		}
	}
}

// remove client from the client array in a room
void room_user_remove(room_t *room, uint8_t *user, size_t len) {
	client_t **clients = room->client_array;
	for(int i = 0; i < MAX_CLIENTS; i ++) {
		if(clients[i]->sockfd > 0) {
			if(memcmp(user, clients[i]->user_name, len) == 0) {
				clients[i]->sockfd = 0;
				clients[i]->user_name = NULL;
				clients[i]->room = NULL;
				//free(clients[i]);
				break;
			} 
		}
	}
}

// add client to the specified room, returns 1 if success, 0 other wise
int room_user_add(room_t *room, uint8_t *password, size_t password_len, client_t *client) {
	client_t **clients = room->client_array;

	if((password == NULL && room->password == NULL) || 
		((memcmp(room->password, password, strlen((char*) room->password)) == 0) 
		&& password_len == strlen((char*) room->password))) {	
		for(int i = 1; i < MAX_CLIENTS; i ++) {
			if(clients[i]->sockfd == 0) {
				clients[i]->sockfd = client->sockfd;
				clients[i]->user_name = client->user_name;
				clients[i]->room = client->room;
				break;
			}
		}
		return 1;
	} else {
		return 0;
	}
}

// change nick in the specified room
void change_nick_in_room(room_t *room, uint8_t *old_user, size_t old_user_len, uint8_t *new_user, size_t new_user_len) {

	client_t **clients = room->client_array;
	for(int i = 0; i < MAX_CLIENTS; i ++) {
		if(clients[i]->sockfd > 0) {
			if(memcmp(old_user, clients[i]->user_name, old_user_len) == 0) {
				uint8_t *updated_name = malloc(new_user_len);
				updated_name = new_user;
				clients[i]->user_name = updated_name;
				break;
			}
		}
	}
}	

