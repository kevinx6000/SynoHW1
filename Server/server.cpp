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
#include <fcntl.h>
#include <errno.h>
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
	this->is_served_.clear();
	this->port_ = port;
	while (!this->client_que_.empty()) this->client_que_.pop();

	// Create mutex and conditional variables
	pthread_mutex_init(&this->served_mutex_, NULL);
	pthread_mutex_init(&this->alive_mutex_, NULL);
	pthread_mutex_init(&this->que_mutex_, NULL);
	pthread_cond_init(&this->que_not_empty_, NULL);
}

// Make file descriptor to nonblocking
bool Server::MakeNonblocking(int fd) {

	// Get current flag
	int file_flags = fcntl(fd, F_GETFL, 0);
	if (file_flags == -1) {
		perror("[Error] Get socket flags failed");
		return false;
	}
	file_flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, file_flags) == -1) {
		perror("[Error] Set socket as non-blocking failed");
		return false;
	}
	return true;
}

// Create socket
bool Server::CreateSocket(void) {

	// Create socket
	this->server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (this->server_socket_ == -1) {
		perror("[Error] Create socket failed");
		return false;
	}

	// Make server socket into non-blocking
	if (!MakeNonblocking(this->server_socket_)) {
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
	int bind_result = bind(this->server_socket_,
			(struct sockaddr *)&server_addr, sizeof(server_addr));
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
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = server_socket_;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, this->server_socket_, &ev) == -1) {
		perror("[Error] epoll_ctl");
		return false;
	}

	// Server control 
	int numFD = 0;
	struct epoll_event events[MAX_EVENT];
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
	while(!abort_flag_) {

		// Wait until some fd is ready
		numFD = epoll_wait(epoll_fd, events, MAX_EVENT, 2000);
		if (numFD == -1 && !abort_flag_) {
			perror("[Error] epoll_wait");
			return false;
		}

		if (abort_flag_) break;

		// Process all ready events
		for(int i = 0; i < numFD; i++) {
			
			// Server socket event
			if (events[i].data.fd == this->server_socket_) {

				// Accept one or more client
				int client_socket = -1;
				while ((client_socket = accept(this->server_socket_, 
						(struct sockaddr *) &client_addr, &addrlen)) >= 0) {

					// Exceed maximum available fd, close
					if (client_socket >= MAX_FD) {
						fprintf(stderr, "[Warning] fd >= %d, socket closed.\n", MAX_FD);
						close(client_socket);
						continue;
					}

					// Make client socket into non-blocking
					if (!MakeNonblocking(client_socket)) {
						close(client_socket);
						continue;
					}

					// Record into epoll
					ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
					ev.data.fd = client_socket;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &ev) == -1) {
						perror("[Warning] epoll_ctl: client socket");
					} else {

						// Cannot allocate enough memory
						char *buf_tmp = (char *)calloc(MAX_BUF, sizeof(char));
						if (buf_tmp == NULL) {
							fprintf(stderr, "[Warning] Memory allocation failed for %d, close\n", client_socket);
							close(client_socket);
							continue;
						}

						// Flush content and set status to READ
						this->content_[client_socket].byte = 0;
						this->content_[client_socket].str = buf_tmp;
						this->content_[client_socket].status = READ;

						// Update alive mark
						pthread_mutex_lock(&this->alive_mutex_);
						this->is_alive_[client_socket] = true;
						pthread_mutex_unlock(&this->alive_mutex_);
						fprintf(stderr, "[Info] Client %d accepted.\n", client_socket);
					}
				}

				// Error other than EAGAIN and EWOULDBLOCK
				if (errno != EAGAIN && errno != EWOULDBLOCK) {
					perror("[Error] Accept failed");
				}

			// Client socket event (I/O or close)
			} else {

				// Retieve event details
				JobNode jNode;
				jNode.fd = events[i].data.fd;
				jNode.events = events[i].events;

				// Push client fd into queue, and signal
				pthread_mutex_lock(&this->que_mutex_);
				this->client_que_.push(jNode);
				pthread_cond_signal(&this->que_not_empty_);
				pthread_mutex_unlock(&this->que_mutex_);
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

// Thread function to serve client
void *Server::ServeClient(void *para) {
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
		JobNode jNode;
		if (!ptr->abort_flag_) {

			// Queue is not empty, extract one client socket
			jNode = ptr->client_que_.front();
			ptr->client_que_.pop();
		}

		// Unlock mutex: free access to everyone
		pthread_mutex_unlock(&ptr->que_mutex_);

		// SIGTERM
		if (ptr->abort_flag_) break;

		// Check alive:
		// If client fd is not alive, should not serve it (discard event)
		bool is_alive = false;
		pthread_mutex_lock(&ptr->alive_mutex_);
		is_alive = ptr->is_alive_[jNode.fd];
		pthread_mutex_unlock(&ptr->alive_mutex_);
		if (!is_alive) continue;

		// Check served:
		// If client fd is being served, should not serve it (push to queue)
		bool is_served = false;
		pthread_mutex_lock(&ptr->served_mutex_);
		is_served = ptr->is_served_[jNode.fd];
		if (!is_served) ptr->is_served_[jNode.fd] = true;
		pthread_mutex_unlock(&ptr->served_mutex_);
		if (is_served) {

			// Push back into queue
			pthread_mutex_lock(&ptr->que_mutex_);
			ptr->client_que_.push(jNode);
			pthread_mutex_unlock(&ptr->que_mutex_);
			continue;
		}

		// Check current status
		// If status is not consistent, should not serve it (discard event)
		Status cur_status;
		cur_status = ptr->content_[jNode.fd].status;
		if ((cur_status == READ && !(jNode.events & EPOLLIN))
			|| (cur_status == WRITE && !(jNode.events & EPOLLOUT))){

		// Otherwise, serve it!
		} else {

			// Read event (and write if completed)
			if (cur_status == READ) {
				ptr->ReadFromClient(jNode.fd);

			// Write event
			} else if (cur_status == WRITE) {
				ptr->WriteToClient(jNode.fd);

			// Unknown event??
			} else {
				fprintf(stderr, "[Error] Unknown event id = %d\n", cur_status);
				fflush(stderr);
			}
		}

		// Mark client fd as not being served
		pthread_mutex_lock(&ptr->served_mutex_);
		ptr->is_served_[jNode.fd] = false;
		pthread_mutex_unlock(&ptr->served_mutex_);
	}
	pthread_exit(NULL);
}

// Read string from client
void Server::ReadFromClient(int client_fd) {
	int cur_byte = 0;
	char *cur_content = NULL;

	// Retrieve current content of string (read/write)
	cur_byte = this->content_[client_fd].byte;
	cur_content = this->content_[client_fd].str;

	// Read until byte reaches target
	int need_byte = sizeof(char) * MAX_BUF;
	int read_byte = 0;
	while (cur_byte < need_byte) {
		read_byte = read(client_fd, cur_content + cur_byte, need_byte - cur_byte);

		// SIGTERM
		if (this->abort_flag_) {
			break;
		}

		// Append content
		if (read_byte > 0) {
			cur_byte += read_byte;

		// Close
		} else if (read_byte == 0) {
			break;

		// EAGAIN or Error
		} else {

			// Interrupt other than SIGTERM, restart
			if (errno == EINTR) {
				continue;
			}
			break;
		}
	}

	// Close socket connection
	if (read_byte == 0) {

		// Close socket
		if (!this->CloseClientSocket(client_fd)) {
			perror("[Error] Close client socket failed");
		}
		fprintf(stderr, "[Info] Client %d is disconnected.\n", client_fd);

	// Read error
	} else if (read_byte == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		perror("[Error] Read error");

		// Close socket
		if (!this->CloseClientSocket(client_fd)) {
			perror("[Error] Close client socket failed");
		}
		fprintf(stderr, "[Info] Server terminates connection to client %d\n", client_fd);

	// Read successfully
	} else {
		
		// Not yet finished
		if (cur_byte < need_byte) {
			this->content_[client_fd].byte = cur_byte;

		// Finished, change to WRITE mode
		} else {
			this->content_[client_fd].status = WRITE;
			this->content_[client_fd].byte = 0;
			this->WriteToClient(client_fd);
		}
	}
}

// Write string to client
void Server::WriteToClient(int client_fd) {
	int cur_byte = 0;
	char *cur_content = NULL;

	// Retrieve current content of string (read/write)
	cur_byte = this->content_[client_fd].byte;
	cur_content = this->content_[client_fd].str;

	// Write until byte reaches target
	int need_byte = sizeof(char) * MAX_BUF;
	int write_byte = 0;
	while (cur_byte < need_byte) {
		write_byte = write(client_fd, cur_content + cur_byte, need_byte - cur_byte);

		// SIGTERM
		if (this->abort_flag_) {
			break;
		}

		// Update remaining
		if (write_byte > 0) {
			cur_byte += write_byte;

		// EAGAIN or Error
		} else {

			// Interrupt other than SIGTERM, restart
			if (write_byte == -1 && errno == EINTR) {
				continue;
			}
			break;
		}
	}

	// Write error
	if (write_byte == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		perror("[Error] Write error");

		// Close socket
		if (!this->CloseClientSocket(client_fd)) {
			perror("[Error] Close client socket failed");
		}
		fprintf(stderr, "[Info] Server terminates connection to client %d.\n", client_fd);

	// Write successfully
	} else {

		// Not yet finished
		if (cur_byte < need_byte) {
			this->content_[client_fd].byte = cur_byte;

		// Finished, change to READ mode
		} else {
			memset(cur_content, 0, sizeof(char) * MAX_BUF);
			this->content_[client_fd].status = READ;
			this->content_[client_fd].byte = 0;
		}
	}
}

// Close client socket
bool Server::CloseClientSocket(int client_fd) {

	// Update alive mark
	pthread_mutex_lock(&this->alive_mutex_);
	this->is_alive_[client_fd] = false;
	pthread_mutex_unlock(&this->alive_mutex_);

	// Free up content
	if (this->content_[client_fd].str != NULL) {
		free(this->content_[client_fd].str);
		this->content_[client_fd].str = NULL;
	}

	// Close socket
	if (close(client_fd) == -1) {
		return false;
	}
	return true;
}

// Close all socket
bool Server::CloseAllSocket(void) {

	// Client sockets
	std::map<unsigned int, bool>::iterator m_itr;
	for (m_itr = this->is_alive_.begin(); m_itr != this->is_alive_.end(); m_itr++) {
		bool is_alive = m_itr->second;

		// Still alive
		if (is_alive) {
			int client_fd = m_itr->first;
			if (!this->CloseClientSocket(client_fd)) {
				perror("[Error] Close client socket during signal handler failed");
				return false;
			} else {
				fprintf(stderr, "[Info] Client socket %d closed safely.\n", client_fd);
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

// Destructor
Server::~Server(void) {

	// Clear up map and queue
	this->is_served_.clear();
	this->is_alive_.clear();
	while (!this->client_que_.empty()) this->client_que_.pop();

	// Destroy mutex and conditional variable
	pthread_mutex_destroy(&this->served_mutex_);
	pthread_mutex_destroy(&this->alive_mutex_);
	pthread_mutex_destroy(&this->que_mutex_);
	pthread_cond_destroy(&this->que_not_empty_);
}
