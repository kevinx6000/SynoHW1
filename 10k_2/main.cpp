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
bool CWR(int [], int, struct sockaddr_in);
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

	// Connect, write, read
	if (!CWR(sockets, MAX_CLIENT, server_addr)) {
		perror("[Error] exp failed");
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

// Connect, send, receive at once
bool CWR(int sockets[], int num, struct sockaddr_in server_addr) {
	int need_byte = sizeof(char) * BUF_SIZE;
	int write_byte = 0;
	int read_byte = 0;
	int cur_byte = 0;
	char *send = (char *)calloc(BUF_SIZE, sizeof(char));
	char *recv = (char *)calloc(BUF_SIZE, sizeof(char));
	for(int i = 0; i < num; i++) {
		if (connect(sockets[i], (struct sockaddr *)&server_addr,
				sizeof(server_addr)) == -1) {
			return false;
		}
		cur_byte = 0;
		sprintf(send, "hi %d", i);
		while (cur_byte < need_byte) {
			write_byte = write(sockets[i], send + cur_byte, need_byte - cur_byte);
			if (write_byte == -1) {
				return false;
			}
			cur_byte += write_byte;
		}
		cur_byte = 0;
		while (cur_byte < need_byte) {
			read_byte = read(sockets[i], recv + cur_byte, need_byte - cur_byte);
			if (read_byte == -1) {
				return false;
			}
			cur_byte += read_byte;
		}
		printf("%d: %s\n", i, recv);
		fflush(stdout);
	}
	free(send);
	free(recv);
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
