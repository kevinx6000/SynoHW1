#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>

// Create socket
bool Client::CreateSocket(void) {

	// Create socket
	this->client_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (this->client_socket_ == -1) {
		fprintf(stderr, "[Error] Create socket failed\n");
		return false;
	}

	// Set server address
	memset(&this->server_addr_, 0, sizeof(this->server_addr_));
	this->server_addr_.sin_family = AF_INET;
	this->server_addr_.sin_addr.s_addr = inet_addr(this->kServerIP);
	this->server_addr_.sin_port = htons(9453);

	// All pass
	return true;
}

// Connect to server
bool Client::Connect(void) {
	int conn_result = connect(this->client_socket_,
			(struct sockaddr *)&this->server_addr_, sizeof(this->server_addr_));
	if (conn_result == -1) {
		fprintf(stderr, "[Error] Connection failed\n");
		return false;
	} else {
		return true;
	}
}

// Send string to server
bool Client::SendString(std::string input_string) {

	// Check string length
	if (input_string.length() >= this->kMaxStringLength) {
		fprintf(stderr, "[Error] Length of input string is too long.\n");
		return false;
	} else {

		// Step1. Send string length to server
		char token[this->kTokenLength];
		memset(token, 0, sizeof(token));
		sprintf(token, "%lu", input_string.length());
		if (write(this->client_socket_, token, this->kTokenLength) == -1) {
			fprintf(stderr, "[Error] String lenght sending failed\n");
			return false;
		}

		// Step2. Send the actual string
		char *send_string = (char *)malloc(sizeof(char) * (input_string.length()+1));
		sprintf(send_string, "%s", input_string.c_str());
		if (write(this->client_socket_, send_string, input_string.length()) == -1) {
			fprintf(stderr, "[Error] String content sending failed\n");
			return false;
		}
		free(send_string);

		// All pass
		return true;
	}
}

// Close socket
bool Client::CloseSocket(void) {
	if (close(this->client_socket_) == -1) {
		fprintf(stderr, "[Error] Close socket failed\n");
		return false;
	} else {
		return true;
	}
}
