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
#include <queue>

// Constructor
Server::Server(void) {
	this->Initialize(9453);
}
Server::Server(int port) {
	this->Initialize(port);
}

// Set abort flag
void Server::SetAbortFlag(bool cur_flag) {
	this->abort_flag_ = cur_flag;
}

// Initialize
void Server::Initialize(int port) {

	// Initialize variables
	this->abort_flag_ = false;
	this->server_socket_ = -1;
	this->is_alive_.clear();
	this->port_ = port;
	while (!this->client_que_.empty()) this->client_que_.pop();

	// Create mutex and conditional variables
	pthread_mutex_init(&this->map_mutex_, NULL);
	pthread_mutex_init(&this->que_mutex_, NULL);
	pthread_cond_init(&this->que_not_empty_, NULL);
}

// Create socket
bool Server::CreateSocket(void) {

	// Create socket
	this->server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (this->server_socket_ == -1) {
		perror("[Error] Create socket failed");
		return false;
	}

	// Set address
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(this->port_);

	// Prevent kernel hold the address used previously
	int mark = 1;
	if (setsockopt(this->server_socket_, SOL_SOCKET, SO_REUSEADDR, &mark,
			sizeof(mark)) == -1) {
		perror("[Error] Set socket option failed");
		return false;
	}

	// Bind address
	int bind_result = bind(this->server_socket_, (struct sockaddr *)&server_addr,
			sizeof(server_addr));
	if (bind_result == -1) {
		perror("[Error] Bind address failed");
		return false;
	}

	// Set to listen mode
	int listen_result = listen(this->server_socket_, MAX_CLIENT);
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

	// Create MAX_THREAD threads
	for (int i = 0; i < MAX_THREAD; i++) {
		if (pthread_create(&this->pid[i], NULL, ServeClient, (void *)this) != 0) {
			perror("[Error] Thread create failed");
			return false;
		}
	}
	
	// Create epoll
	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		perror("[Error] epoll_create1");
		return false;
	}

	// Monitor server socket
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = server_socket_;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, this->server_socket_, &ev) == -1) {
		perror("[Error] epoll_ctl");
		return false;
	}

	// Server control 
	int numFD;
	struct epoll_event events[MAX_EVENT];
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
	while(!abort_flag_) {

		// Wait until some fd is ready
		numFD = epoll_wait(epoll_fd, events, MAX_EVENT, 3000);
		if (numFD == -1 && !abort_flag_) {
			perror("[Error] epoll_wait");
			return false;
		}

		// Process all ready events
		for(int i = 0; i < numFD && !abort_flag_; i++) {
			
			// Server socket event
			if (events[i].data.fd == this->server_socket_) {

				// Accept one client and record into epoll
				int client_socket = accept(this->server_socket_,
					(struct sockaddr *) &client_addr, &addrlen);
				if (client_socket == -1) {
					perror("[Warning] Accept connection failed");
				} else {
					ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
					ev.data.fd = client_socket;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &ev) == -1) {
						perror("[Warning] epoll_ctl: client socket");
					} else {

						// Update alive mark
						pthread_mutex_lock(&this->map_mutex_);
						this->is_alive_[client_socket] = true;
						pthread_mutex_unlock(&this->map_mutex_);
						fprintf(stderr, "[Info] Client %d accepted.\n", client_socket);
					}
				}

			// Client socket event
			} else {

				// Socket close event
				if (events[i].events & EPOLLRDHUP) {
					fprintf(stderr, "[Info] Client %d is disconnected.\n", events[i].data.fd);

					// Update alive mark
					pthread_mutex_lock(&this->map_mutex_);
					this->is_alive_[events[i].data.fd] = false;
					pthread_mutex_unlock(&this->map_mutex_);

					// Remove from epoll
					if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) == -1) {
						perror("[Warning] epoll_ctl: remove client socket");
					}

					// Close socket
					if (close(events[i].data.fd) == -1) {
						perror("[Error] Close client socket failed");
					}

				// I/O ready
				} else {

					// Push client fd into queue, and signal
					pthread_mutex_lock(&this->que_mutex_);
					this->client_que_.push(events[i].data.fd);
					pthread_cond_signal(&this->que_not_empty_);
					pthread_mutex_unlock(&this->que_mutex_);
				}
			}
		}
	}

	// Wait for all threads join
	pthread_cond_broadcast(&this->que_not_empty_);
	for (int i = 0; i < MAX_THREAD; i++) {
		pthread_join(this->pid[i], NULL);
	}

	// All operation finished safely
	return true;
}

// Close socket
bool Server::CloseSocket(void) {

	// Client sockets
	std::map<unsigned int, bool>::iterator m_itr;
	for (m_itr = this->is_alive_.begin(); m_itr != this->is_alive_.end(); m_itr++) {

		// Still alive
		if (m_itr->second) {
			if (close(m_itr->first) == -1) {
				perror("[Error] Close client socket during signal handler failed");
				return false;
			} else {
				fprintf(stderr, "[Info] Client socket %d closed safely.\n", m_itr->first);
			}
		}
	}

	// Server socket
	if (close(this->server_socket_) == -1) {
		perror("[Error] Close socket failed");
		return false;
	} else {
		fprintf(stderr, "[Info] Server socket closed safely.\n");
		this->server_socket_ = -1;
	}

	return true;
}

// Thread function to serve client
void *Server::ServeClient(void *para) {
	char recv_string[kBufSiz];
	Server *ptr = (Server *)para;

	// Thread loop
	while (!ptr->abort_flag_) {

		// Lock mutex: get access to client queue
		pthread_mutex_lock(&ptr->que_mutex_);

		// Check if queue is empty, and wait if so
		while (ptr->client_que_.empty()) {
			pthread_cond_wait(&ptr->que_not_empty_, &ptr->que_mutex_);
			if (ptr->abort_flag_) break;
		}

		// Not caused by SIGTERM
		int client_fd = -1;
		if (!ptr->abort_flag_) {

			// Queue is not empty, extract one client socket
			client_fd = ptr->client_que_.front();
			ptr->client_que_.pop();
		}

		// Unlock mutex: free access to everyone
		pthread_mutex_unlock(&ptr->que_mutex_);

		// SIGTERM
		if (ptr->abort_flag_) break;

		// Handle only when client is alive
		pthread_mutex_lock(&ptr->map_mutex_);
		if (ptr->is_alive_[client_fd]) {

			// Receive string directly
			// Will not block since epoll is ready before entering thread
			int read_len = read(client_fd, recv_string, sizeof(char) * kBufSiz);

			// Client really sent string
			if (read_len > 0) {
				fprintf(stderr, "[Info] String = %s (client %d)\n", recv_string, client_fd);

				// Send back string directly
				int write_len = write(client_fd, recv_string, sizeof(char) * kBufSiz);
				if (write_len <= 0) {
					perror("[Error] Send back string content failed");
				}
				fprintf(stderr, "[Info] String sent back (client %d)\n", client_fd);
				if (read_len != write_len) {
					fprintf(stderr, "!!!: %d vs %d\n", read_len, write_len);
					fflush(stderr);
				}

			// IMPOSSIBLE CONDITION
			} else {
				perror("[Error] Read failed (impossible?)");
			}
		}
		pthread_mutex_unlock(&ptr->map_mutex_);
	}
	pthread_exit(NULL);
}

// Destructor
Server::~Server(void) {

	// Clear up map and queue
	this->is_alive_.clear();
	while (!this->client_que_.empty()) this->client_que_.pop();

	// Destroy mutex and conditional variable
	pthread_mutex_destroy(&this->map_mutex_);
	pthread_mutex_destroy(&this->que_mutex_);
	pthread_cond_destroy(&this->que_not_empty_);
}
