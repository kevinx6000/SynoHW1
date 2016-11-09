#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

// Static class members
int Server::server_socket_;
int Server::client_socket_[MAX_CLIENT];
int Server::client_cnt_;

// Constructor
Server::Server(void) {

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
	server_socket_ = -1;
	for(int i = 0; i < MAX_CLIENT; i++) {
		client_socket_[i] = -1;
	}
	client_cnt_ = MAX_CLIENT;
}

// Create socket
bool Server::CreateSocket(void) {

	// Create socket
	server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket_ == -1) {
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
	if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &mark,
			sizeof(mark)) == -1) {
		perror("[Error] Set socket option failed");
		return false;
	}

	// Bind address
	int bind_result = bind(server_socket_, (struct sockaddr *)&this->server_addr_,
			sizeof(this->server_addr_));
	if (bind_result == -1) {
		perror("[Error] Bind address failed");
		return false;
	}

	// Set to listen mode
	int listen_result = listen(server_socket_, MAX_CLIENT);
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

	// Server control loop
	while(true) {

		// Polling for available client socket slot
		while(this->client_cnt_ <= 0) {
			fprintf(stderr, "[Info] # of client reached maximum...sleep 3 secs\n");
			sleep(3);
		}

		// Allocate one client socket
		int cid = -1;
		for(int i = 0; i < MAX_CLIENT; i++) {
			if (client_socket_[i] == -1) {
				cid = i;
				break;
			}
		}
		if (cid == -1) {
			fprintf(stderr, "[Error] No remaining client socket found\n");
			exit(EXIT_FAILURE);
		}

		// Waiting for connection from client
		fprintf(stderr, "[Info] Waiting for connection from client...\n");
		socklen_t addrlen = sizeof(this->client_addr_);
		client_socket_[cid] = accept(server_socket_,
			(struct sockaddr *)&this->client_addr_, &addrlen);

		// Check connection result
		if (client_socket_[cid] == -1) {
			perror("[Error] Accept connection failed");
			return false;
		} else {

			// Flag: connection success
			client_cnt_--;
			fprintf(stderr, "[Info] Connection success for client %d.\n", cid);

			// Create a thread to serve client
			int *tmpID = (int *)malloc(sizeof(int));
			*tmpID = cid;
			pthread_t pid;
			if (pthread_create(&pid, NULL, ServeClient, (void *)tmpID) != 0) {
				perror("Thread create failed");
				exit(EXIT_FAILURE);
			}
		}
	}
	return true;
}

// Close socket
bool Server::CloseSocket(void) {
	if (close(server_socket_) == -1) {
		perror("Close socket failed");
		return false;
	} else {
		server_socket_ = -1;
		return true;
	}
}

// Signal handler
void Server::SignalHandler(int signum) {

	// Close all client sockets
	for(int i = 0; i < MAX_CLIENT; i++) {
		if (client_socket_[i] != -1) {
			if (close(client_socket_[i]) == -1) {
				perror("[Error] Close client socket during signal handler failed");
				exit(EXIT_FAILURE);
			} else {
				fprintf(stderr, "[Info] Client socket %d has been safely closed.\n", i);
				client_socket_[i] = -1;
			}
		}
	}
	
	// Close server socket
	if (server_socket_ != -1) {
		if (close(server_socket_) == -1) {
			perror("[Error] Close server socket during signal handler failed");
			exit(EXIT_FAILURE);
		} else {
			fprintf(stderr, "[Info] Server socket has been safely closed.\n");
			server_socket_ = -1;
		}
	}

	// Exit with success flag
	exit(EXIT_SUCCESS);
}

// Thread function to serve client
void *Server::ServeClient(void *para) {

	// ID of client socket
	int cid = *((int *)para);
	free(para);

	// Receive string directly
	char *recv_string = (char *)calloc(kBufSiz, sizeof(char));
	if (read(client_socket_[cid], recv_string, sizeof(char) * kBufSiz) == -1) {
		perror("[Error] Receive string content failed");
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "[Info] String = %s (client %d)\n", recv_string, cid);

	// Send back string directly
	if (write(client_socket_[cid], recv_string, sizeof(char) * kBufSiz) == -1) {
		perror("[Error] Send back string content failed");
		exit(EXIT_FAILURE);
	}
	
	// Done
	fprintf(stderr, "[Info] String sent back (client %d)\n", cid);
	free(recv_string);
	if (close(client_socket_[cid]) == -1) {
		perror("[Error] Close client socket failed");
		exit(EXIT_FAILURE);
	}
	client_socket_[cid] = -1;
	client_cnt_++;
	return NULL;
}
