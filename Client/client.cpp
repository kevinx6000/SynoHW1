#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>

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
Client::Client(const char *IP, const int port) {
	this->Initialize(IP, port);
}

// Initialize variable
void Client::Initialize(const char *IP, const int port) {
	is_abort_ = false;
	this->client_socket_ = -1;
	this->server_IP_ = IP;
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
	this->server_addr_.sin_addr.s_addr = inet_addr(this->server_IP_.c_str());
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
	if (!is_abort_) {
		if (!this->SendString(input)) {
			return false;
		}
	}

	// Receive
	if (!is_abort_) {
		if (!this->RecvString(output)) {
			return false;
		}
	}

	return true;
}

// Send string to server
bool Client::SendString(const std::string &input_string) {

	// Input length too long
	if (input_string.length() >= Client::kMaxBuf) {
		fprintf(stderr, "[Error] Length of input string is too long.\n");
		return false;
	
	// Send it
	} else {

		// Send string directly
		int need_byte = sizeof(char) * Client::kMaxBuf;
		int write_byte = 0;
		int cur_byte = 0;
		std::string send_string = input_string;
		send_string.resize(Client::kMaxBuf, 0);
		while (cur_byte < need_byte) {
			write_byte = write(this->client_socket_, send_string.c_str() + cur_byte, need_byte - cur_byte);

			// SIGTERM
			if (is_abort_) {
				return false;
			}

			// Error
			if (write_byte == -1) {

				// Interrupt other than SIGTERM: restart
				if (errno == EINTR) {
					continue;

				// Other fail
				} else {
					perror("[Error] Write failed");
					return false;
				}
			}
			cur_byte += write_byte;
		}

		// All pass
		return true;
	}
}

// Receive string from server
bool Client::RecvString(std::string &output_string) {

	// Receive string directly
	int need_byte = sizeof(char) * Client::kMaxBuf;
	int read_byte = 0;
	int cur_byte = 0;
	bool is_fail = false;
	char *recv_string = (char *)calloc(Client::kMaxBuf, sizeof(char));
	while (cur_byte < need_byte) {
		read_byte = read(this->client_socket_, recv_string + cur_byte, need_byte - cur_byte);

		// SIGTERM
		if (is_abort_) {
			is_fail = true;
			break;
		}

		// Error
		if (read_byte == -1) {

			// Interrupt other than SIGTERM
			if (errno == EINTR) {
				continue;

			// Other fail
			} else {
				perror("[Error] Read failed");
				is_fail = true;
				break;
			}
		}
		cur_byte += read_byte;
	}

	// Copy output if not failed
	if (!is_fail) {
		output_string = recv_string;
	}
	free(recv_string);

	// Return result
	return !is_fail;
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

// Destructor
Client::~Client(void) {
	// Do nothing
}

// Get abort flag
bool Client::GetAbortFlag(void) {
	return is_abort_;
}

// Set abort flag
void Client::SetAbortFlag(bool flag) {
	is_abort_ = flag;
}
