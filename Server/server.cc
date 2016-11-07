#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// Create socket
bool Server::CreateSocket(void) {

	// Create socket
	this->server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (this->server_socket_ == -1) {
		perror("[Error] Create socket failed");
		return false;
	}

	// Set address
	memset(&this->server_addr_, 0, sizeof(this->server_addr_));
	this->server_addr_.sin_family = AF_INET;
	this->server_addr_.sin_addr.s_addr = INADDR_ANY;
	this->server_addr_.sin_port = htons(9453);

	// Prevent kernel hold the address used previously
	int mark = 1;
	if (setsockopt(this->server_socket_, SOL_SOCKET, SO_REUSEADDR, &mark,
			sizeof(mark)) == -1) {
		perror("[Error] Set socket option failed");
		return false;
	}

	// Bind address
	int bind_result = bind(this->server_socket_, (struct sockaddr *)&this->server_addr_,
			sizeof(this->server_addr_));
	if (bind_result == -1) {
		perror("[Error] Bind address failed");
		return false;
	}

	// Set to listen mode
	int listen_result = listen(this->server_socket_, this->kMaxConnection);
	if (listen_result == -1) {
		perror("[Error] Listen failed");
		return false;
	}

	// Flag: socket created
	fprintf(stderr, "[Info] Socket created successfully.\n");

	// All pass
	return true;
}

// Accept connection from client
bool Server::AcceptConnection(void) {

	// Wait for connection from client (block)
	socklen_t addrlen = sizeof(this->client_addr_);
	this->client_socket_ = accept(this->server_socket_,
			(struct sockaddr *)&this->client_addr_, &addrlen);

	// Check connection result
	if (this->client_socket_ == -1) {
		perror("[Error] Accept connection failed");
		return false;
	} else {

		// Flag: connection success
		fprintf(stderr, "[Info] Connection success.\n");

		// Echo client
		if (!this->EchoString()) {
			fprintf(stderr, "[Error] Echo string failed.\n");
			return false;
		}

		// Close client connection
		if (close(this->client_socket_) == -1) {
			perror("[Error] Close client socket failed");
			return false;
		}
		return true;
	}
}

// Echo string from client
bool Server::EchoString(void) {

	// Step1. Receive the length of string
	int strlen;
	char token[this->kTokenLength];
	memset(token, 0, sizeof(token));
	if (read(this->client_socket_, token, sizeof(char) * this->kTokenLength) == -1) {
		perror("[Error] Receive string length failed");
		return false;
	}
	sscanf(token, "%d", &strlen);
	fprintf(stderr, "[Info] String length = %d\n", strlen);

	// Step2. Receive the conent of string
	char *recv_string = (char *)malloc(sizeof(char) * (strlen+1));
	if (read(this->client_socket_, recv_string, sizeof(char) * strlen) == -1) {
		perror("[Error] Receive string content failed");
		return false;
	}
	recv_string[strlen] = '\0';
	fprintf(stderr, "[Info] String = %s\n", recv_string);

	// Step3. Send back the length of string
	if (write(this->client_socket_, token, sizeof(char) * this->kTokenLength) == -1) {
		perror("[Error] Send back string length failed");
		return false;
	}

	// Step4. Send back the content of string
	if (write(this->client_socket_, recv_string, sizeof(char) * strlen) == -1) {
		perror("[Error] Send back string content failed");
		return false;
	}
	
	// Done
	fprintf(stderr, "[Info] String sent back.\n");
	free(recv_string);
	return true;
}

// Close socket
bool Server::CloseSocket(void) {
	if (close(this->server_socket_) == -1) {
		perror("Close socket failed");
		return false;
	} else {
		return true;
	}
}
