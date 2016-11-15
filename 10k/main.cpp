// Headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Macros
#define MAX_CLIENT 10000
#define BUF_SIZE 1023

// Functions
void SetServerAddr(struct sockaddr_in &);
bool CreateSocket(int [], int);
bool Connect(int [], int, struct sockaddr_in);
bool SendString(int [], int);
bool RecvString(int [], int);
bool CloseSocket(int [], int);

// Main
int main(int argc, char *argv[]) {

	// Set server address
	struct sockaddr_in server_addr;
	SetServerAddr(server_addr);

	// Create sockets
	int sockets[MAX_CLIENT];
	if (!CreateSocket(sockets, MAX_CLIENT)) {
		perror("[Error] Create socket failed");
		exit(EXIT_FAILURE);
	}

	// Connect to server
	if (!Connect(sockets, MAX_CLIENT, server_addr)) {
		perror("[Error] Some socket cannot connect to server");
		exit(EXIT_FAILURE);
	}

	// Send string to server
	if (!SendString(sockets, MAX_CLIENT)) {
		perror("[Error] Some client cannot send string to server");
		exit(EXIT_FAILURE);
	}

	// Receive string from server
	if (!RecvString(sockets, MAX_CLIENT)) {
		perror("[Error] Some client cannot receive string from server");
		exit(EXIT_FAILURE);
	}

	// Close sockets
	if (!CloseSocket(sockets, MAX_CLIENT)) {
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

// Connect to server simultaneously
bool Connect(int sockets[], int num, struct sockaddr_in server_addr) {
	for(int i = 0; i < num; i++) {
		if (connect(sockets[i], (struct sockaddr *)&server_addr,
				sizeof(server_addr)) == -1) {
			return false;
		}
		fprintf(stderr, "[Info] Connet %d done.\n", i);
		fflush(stderr);
	}
	return true;
}

// Send string to server
bool SendString(int sockets[], int num) {
	char *str = (char *)calloc(BUF_SIZE, sizeof(char));
	for(int i = 0; i < num; i++) {
		sprintf(str, "hi %d", i);
		if (write(sockets[i], str, sizeof(char) * BUF_SIZE) == -1) {
			return false;
		}
	}
	free(str);
	return true;
}

// Receive string from server
bool RecvString(int sockets[], int num) {
	char *str = (char *)calloc(BUF_SIZE, sizeof(char));
	for(int i = 0; i < num; i++) {
		if (read(sockets[i], str, sizeof(char) * BUF_SIZE) == -1) {
			return false;
		}
		printf("%d: %s\n", i, str);
		fflush(stdout);
	}
	free(str);
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
