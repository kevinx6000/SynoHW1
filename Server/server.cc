#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <map>

// Static class members
int Server::server_socket_;
std::map<unsigned int, bool> Server::is_alive_;

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

	// Initialize
	server_socket_ = -1;
	is_alive_.clear();
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
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(9453);

	// Prevent kernel hold the address used previously
	int mark = 1;
	if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &mark,
			sizeof(mark)) == -1) {
		perror("[Error] Set socket option failed");
		return false;
	}

	// Bind address
	int bind_result = bind(server_socket_, (struct sockaddr *)&server_addr,
			sizeof(server_addr));
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
	
	// Create epoll
	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		perror("[Error] epoll_create1");
		exit(EXIT_FAILURE);
	}

	// Monitor server socket
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = server_socket_;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket_, &ev) == -1) {
		perror("[Error] epoll_ctl");
		exit(EXIT_FAILURE);
	}

	// Server control 
	int numFD;
	struct epoll_event events[MAX_EVENT];
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
	while(true) {

		// Wait until some fd is ready
		numFD = epoll_wait(epoll_fd, events, MAX_EVENT, -1);
		if (numFD == -1) {
			perror("[Error] epoll_wait");
			exit(EXIT_FAILURE);
		}

		// Process all ready events
		for(int i = 0; i < numFD; i++) {
			
			// Server socket ready
			if (events[i].data.fd == server_socket_) {

				// Accept one client and record into epoll
				fprintf(stderr, "[Info] Accepting new client...\n");
				int client_socket = accept(server_socket_,
					(struct sockaddr *) &client_addr, &addrlen);
				if (client_socket == -1) {
					perror("[Error] Accept connection failed");
					exit(EXIT_FAILURE);
				}
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = client_socket;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &ev) == -1) {
					perror("[Error] epoll_ctl: client socket");
					exit(EXIT_FAILURE);
				}
				// TODO: Mutex
				is_alive_[client_socket] = true;
				fprintf(stderr, "[Info] Client %d accepted.\n", client_socket);

			// Client socket ready
			} else {

				// Create a thread to handle this client
				fprintf(stderr, "[Info] Client %d is ready.\n", events[i].data.fd);
				int *curID = (int *)malloc(sizeof(int));
				*curID = events[i].data.fd;
				pthread_t pid;
				if (pthread_create(&pid, NULL, ServeClient, (void *)curID) != 0) {
					perror("[Error] Thread create failed");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
	return true;
}

// Close socket
bool Server::CloseSocket(void) {
	if (close(server_socket_) == -1) {
		perror("[Error] Close socket failed");
		return false;
	} else {
		server_socket_ = -1;
		return true;
	}
}

// Signal handler
void Server::SignalHandler(int signum) {

	// Close all client sockets
	std::map<unsigned int, bool>::iterator m_itr;
	for (m_itr = is_alive_.begin(); m_itr != is_alive_.end(); m_itr++) {

		// Still alive
		if (m_itr->second) {
			if (close(m_itr->first) == -1) {
				perror("[Error] Close client socket during signal handler failed");
				exit(EXIT_FAILURE);
			} else {
				fprintf(stderr, "[Info] Client socket %d closed safely.\n", m_itr->first);
			}
		}
	}
	
	// Close server socket
	if (server_socket_ != -1) {
		if (close(server_socket_) == -1) {
			perror("[Error] Close server socket during signal handler failed");
			exit(EXIT_FAILURE);
		} else {
			fprintf(stderr, "[Info] Server socket closed safely.\n");
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
	fprintf(stderr, "[Info] Client %d is handled by thread.\n", cid);

	// Receive string directly
	char *recv_string = (char *)calloc(kBufSiz, sizeof(char));
	int read_len = read(cid, recv_string, sizeof(char) * kBufSiz);
	if (read_len == -1) {
		perror("[Error] Receive string content failed");
		exit(EXIT_FAILURE);
	}

	// Client really sent string (not disconnect)
	if (read_len > 0) {
		fprintf(stderr, "[Info] String = %s (client %d)\n", recv_string, cid);

		// Send back string directly
		if (write(cid, recv_string, sizeof(char) * kBufSiz) == -1) {
			perror("[Error] Send back string content failed");
			exit(EXIT_FAILURE);
		}
		fprintf(stderr, "[Info] String sent back (client %d)\n", cid);
	} else {
		fprintf(stderr, "[Info] Client %d is probably disconnected.\n", cid);
	}
	
	// Done
	free(recv_string);
	if (close(cid) == -1) {
		perror("[Error] Close client socket failed");
		exit(EXIT_FAILURE);
	}
	// TODO: Mutex
	is_alive_[cid] = false;
	return NULL;
}
