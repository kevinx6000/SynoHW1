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
bool Client::is_sigterm_;

// Constructor
Client::Client(void) {
	this->Initialize("10.13.21.117", 9453);
}
Client::Client(const int port) {
	this->Initialize("10.13.21.117", port);
}
Client::Client(const char *IP) {
	this->Initialize(IP, 9453);
}

// Copy constructor
Client::Client(const Client &other) {
	this->Initialize(other.server_IP_, other.server_port_);
}

// Copy assignment
Client &Client::operator=(const Client &other) {
	
	// Prevent self-assignment
	if (&other != this) {

		// Destroy old variables
		free(this->server_IP_);

		// Initialize again
		this->Initialize(other.server_IP_, other.server_port_);
	}
	return *this;
}

// Initialize variable
void Client::Initialize(const char *IP, const int port) {
	is_sigterm_ = false;
	this->client_socket_ = -1;
	this->server_IP_ = (char *)malloc(sizeof(char)*(strlen(IP)+1));
	strcpy(this->server_IP_, IP);
	this->server_port_ = port;
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
	this->server_addr_.sin_addr.s_addr = inet_addr(this->server_IP_);
	this->server_addr_.sin_port = htons(this->server_port_);

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

// Send to server and receive from server
bool Client::SendAndRecv(const std::string &input, std::string &output) {

	// Send
	if (!is_sigterm_) {
		if (!this->SendString(input)) {
			return false;
		}
	}

	// Receive
	if (!is_sigterm_) {
		if (!this->RecvString(output)) {
			return false;
		}
	}

	return true;
}

// Send string to server
bool Client::SendString(const std::string input_string) {

	// Check string length
	if (input_string.length() >= kBufSiz) {
		fprintf(stderr, "[Error] Length of input string is too long.\n");
		return false;
	} else {

		// Send string directly
		char *send_string = (char *)calloc(kBufSiz, sizeof(char));
		sprintf(send_string, "%s", input_string.c_str());
		if (write(this->client_socket_, send_string, sizeof(char) * kBufSiz) == -1) {
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
	char *recv_string = (char *)calloc(kBufSiz, sizeof(char));
	if (read(this->client_socket_, recv_string, sizeof(char) * kBufSiz) == -1) {
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
		fprintf(stderr, "[Info] Socket has been safely closed.\n");
		client_socket_ = -1;
		return true;
	}
}

// Register signal
bool Client::RegisterSignal(void) {
	struct sigaction new_action;
	new_action.sa_handler = SignalHandler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	if (sigaction(SIGTERM, &new_action, NULL) < 0) {
		perror("Register signal failed");
		return false;
	}
	return true;
}

// Signal handler
void Client::SignalHandler(int signum) {
	is_sigterm_ = true;
}

// Destructor
Client::~Client(void) {

	// Destroy old variables
	free(this->server_IP_);
}

// Get SIGTERM flag
bool Client::GetIsSigterm(void) {
	return is_sigterm_;
}
