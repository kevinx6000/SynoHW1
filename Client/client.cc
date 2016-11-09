#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <iostream>
#include <string>

// Static class members
int Client::client_socket_;

// Constructor
Client::Client(void) :
	kServerIP("10.13.21.117"), kBufSiz(16383) {

	// Register signal
	struct sigaction new_action;
	new_action.sa_handler = this->SignalHandler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	if (sigaction(SIGTERM, &new_action, NULL) < 0) {
		perror("Register signal failed");
		exit(EXIT_FAILURE);
	}

	// Initialize socket
	client_socket_ = -1;
}


// Create socket
bool Client::CreateSocket(void) {

	// Create socket
	this->client_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (this->client_socket_ == -1) {
		perror("[Error] Create socket failed");
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
		perror("[Error] Connection failed");
		return false;
	} else {
		return true;
	}
}

// Send string to server
bool Client::SendString(std::string input_string) {

	// Check string length
	if (input_string.length() >= this->kBufSiz) {
		fprintf(stderr, "[Error] Length of input string is too long.\n");
		return false;
	} else {

		// Send string directly
		char *send_string = (char *)calloc(this->kBufSiz, sizeof(char));
		sprintf(send_string, "%s", input_string.c_str());
		if (write(this->client_socket_, send_string, sizeof(char) * this->kBufSiz) == -1) {
			perror("[Error] String content sending failed");
			return false;
		}
		free(send_string);

		// All pass
		return true;
	}
}

// Receive string from server
bool Client::RecvString(std::string &output_string) {

	// Receive string directly
	char *recv_string = (char *)calloc(this->kBufSiz, sizeof(char));
	if (read(this->client_socket_, recv_string, sizeof(char) * this->kBufSiz) == -1) {
		perror("[Error] Receive string content failed");
		return false;
	}
	output_string = recv_string;
	free(recv_string);
	return true;

}

// Close socket
bool Client::CloseSocket(void) {
	if (close(this->client_socket_) == -1) {
		perror("[Error] Close socket failed");
		return false;
	} else {
		client_socket_ = -1;
		return true;
	}
}

// Signal handler
void Client::SignalHandler(int signum) {

	// Close socket if opened
	if (client_socket_ != -1) {
		if (close(client_socket_) == -1) {
			perror("[Error] Close client socket during signal handler failed");
			exit(EXIT_FAILURE);
		} else {
			fprintf(stderr, "[Info] Socket has been safely closed.\n");
			client_socket_ = -1;
		}
	}

	// Exit with success flag
	exit(EXIT_SUCCESS);
}
