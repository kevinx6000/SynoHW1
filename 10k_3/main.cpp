// Headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

// Macros
#define MAX_CLIENT 10000
#define BUF_SIZE 1023

// Global variable
int gSockets[MAX_CLIENT];
pthread_t gPid[MAX_CLIENT];
struct sockaddr_in gServer_addr;

// Functions
void SetServerAddr(struct sockaddr_in &);
bool CreateSocket(int [], int);
bool Connect(int [], int, struct sockaddr_in);
bool SendString(int [], int);
bool RecvString(int [], int);
bool CloseSocket(int [], int);

// Thread
void *GoTrump(void *);

// Main
int main(int argc, char *argv[]) {

	// Set server address
	SetServerAddr(gServer_addr);

	// Create sockets
	if (!CreateSocket(gSockets, MAX_CLIENT)) {
		perror("[Error] Create socket failed");
		exit(EXIT_FAILURE);
	}

	// Create threads
	int tid[MAX_CLIENT];
	for (int i = 0; i < MAX_CLIENT; i++) {
		tid[i] = i;
		if (pthread_create(&gPid[i], NULL, GoTrump, (void *)&tid[i]) != 0) {
			perror("[Error] Thread create failed");
			exit(EXIT_FAILURE);
		}
	}

	// Join thread
	for (int i = 0; i < MAX_CLIENT; i++) {
		if (pthread_join(gPid[i], NULL) != 0) {
			perror("Thread join");
		}
	}

	// Close sockets
	if (!CloseSocket(gSockets, MAX_CLIENT)) {
		perror("[Error] Close socket failed");
		exit(EXIT_FAILURE);
	}
	return 0;
}

// Set server address
void SetServerAddr(struct sockaddr_in &server_addr) {
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("10.13.21.117");
	server_addr.sin_port = htons(9453);
}

// Create sockets
bool CreateSocket(int sockets[], int num) {
	for(int i = 0; i < num; i++) {
		sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (sockets[i] == -1) {
			return false;
		}
	}
	return true;
}

// Close sockets
bool CloseSocket(int sockets[], int num) {
	for(int i = 0; i < num; i++) {
		if (close(sockets[i]) == -1) {
			return false;
		}
	}
	return true;
}

// Thread
void *GoTrump(void *para) {
	int tid = *((int *)para);

	char *send_str = (char *)calloc(BUF_SIZE, sizeof(char));
	char *recv_str = (char *)calloc(BUF_SIZE, sizeof(char));
	sprintf(send_str, "hi %d", tid);

	// Connect to server
	if (connect(gSockets[tid], (struct sockaddr *)&gServer_addr, 
			sizeof(gServer_addr)) == -1) {
		perror("Connect failed");
		pthread_exit(NULL);
	}
	sleep(1);

	// Until 5 seconds
	int need_byte = sizeof(char) * BUF_SIZE;
	int read_byte = 0;
	int write_byte = 0;
	int cur_byte = 0;
	bool is_failed = false;
	time_t beg_time = time(NULL);
	time_t end_time;
	while ( (end_time = time(NULL)) - beg_time < 3) {

		// Send string
		cur_byte = 0;
		while (cur_byte < need_byte) {
			write_byte = write(gSockets[tid], send_str + cur_byte, need_byte - cur_byte);

			// Error
			if (write_byte == -1) {
				perror("[Error] Write failed");
				is_failed = true;
				break;
			}
			cur_byte += write_byte;
		}
		if (is_failed) break;

		// Recv string
		cur_byte = 0;
		memset(recv_str, 0, sizeof(char) * BUF_SIZE);
		while (cur_byte < need_byte) {
			read_byte = read(gSockets[tid], recv_str + cur_byte, need_byte - cur_byte);

			// Error
			if (read_byte == -1) {
				perror("[Error] Read failed");
				is_failed = true;
				break;
			}
			cur_byte += read_byte;
		}
		if (is_failed) break;

		// Compare
		if (strcmp(send_str, recv_str) != 0) {
			fprintf(stderr, "[Error] (%d) String different %s(%lu) and %s(%lu)\n", tid, send_str, strlen(send_str), recv_str, strlen(recv_str));
			fflush(stderr);
			break;
		}
		usleep(100000);
	}

	// End
	free(send_str);
	free(recv_str);

	pthread_exit(NULL);
}
