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
#include <string>

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
	this->is_used_.clear();
	this->port_ = port;
	while (!this->client_que_.empty()) this->client_que_.pop();

	// Create mutex and conditional variables
	pthread_mutex_init(&this->used_mutex_, NULL);
	pthread_mutex_init(&this->alive_mutex_, NULL);
	pthread_mutex_init(&this->que_mutex_, NULL);
	pthread_mutex_init(&this->content_mutex_, NULL);
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
						fprintf(stderr, "[Info] Client %d accepted.\n", client_socket);

						// Update alive mark
						pthread_mutex_lock(&this->alive_mutex_);
						this->is_alive_[client_socket] = true;
						pthread_mutex_unlock(&this->alive_mutex_);

						// Update status to READ
						pthread_mutex_lock(&this->content_mutex_);
						this->cur_status_[client_socket] = READ;
						this->cur_content_[client_socket] = "";
						this->cur_byte_[client_socket] = 0;
						pthread_mutex_unlock(&this->content_mutex_);
					}
				}

				// Reach end (client_socket == -1) or error
				if (errno == EAGAIN || errno == EWOULDBLOCK) {continue;}
				else {
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
		bool is_used = false;
		pthread_mutex_lock(&ptr->used_mutex_);
		is_used = ptr->is_used_[jNode.fd];
		if (!is_used) ptr->is_used_[jNode.fd] = true;
		pthread_mutex_unlock(&ptr->used_mutex_);
		if (is_used) {

			// Push back into queue
			pthread_mutex_lock(&ptr->que_mutex_);
			ptr->client_que_.push(jNode);
			pthread_mutex_unlock(&ptr->que_mutex_);
			continue;
		}

		// Check current status
		// If status is not consistent, should not serve it (discard event)
		Status cur_status;
		pthread_mutex_lock(&ptr->content_mutex_);
		cur_status = ptr->cur_status_[jNode.fd];
		pthread_mutex_unlock(&ptr->content_mutex_);
		if ((cur_status == READ && !(jNode.events & EPOLLIN))
			|| (cur_status == WRITE && !(jNode.events & EPOLLOUT))){

		// Otherwise, serve it!
		} else {

			// Read event (and write if completed)
			if (cur_status == READ) {
				ptr->ReadFromClient(ptr, jNode);

			// Write event
			} else if (cur_status == WRITE) {
				ptr->WriteToClient(ptr, jNode);

			// Unknown event??
			} else {
				fprintf(stderr, "[Error] Unknown event id = %d\n", cur_status);
				fflush(stderr);
			}
		}

		// Mark client fd as not being served
		pthread_mutex_lock(&ptr->used_mutex_);
		ptr->is_used_[jNode.fd] = false;
		pthread_mutex_unlock(&ptr->used_mutex_);
	}
	pthread_exit(NULL);
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

// Read string from client
void Server::ReadFromClient(Server *ptr, const JobNode &jNode) {

	// Variables
	int cur_byte = 0;
	char str_buf[kBufSiz];
	std::string cur_content = "";

	// Retrieve current content of string (read/write)
	pthread_mutex_lock(&ptr->content_mutex_);
	cur_byte = ptr->cur_byte_[jNode.fd];
	cur_content = ptr->cur_content_[jNode.fd];
	pthread_mutex_unlock(&ptr->content_mutex_);

	// Copy to char buffer
	memset(str_buf, 0, sizeof(str_buf));
	sprintf(str_buf, "%s", cur_content.c_str());

	// Read until byte reaches target
	int need_byte = sizeof(char) * kBufSiz;
	int read_byte = 0;
	while (cur_byte < need_byte) {
		read_byte = read(jNode.fd, str_buf + cur_byte, need_byte - cur_byte);

		// Append content
		if (read_byte > 0) {
			cur_byte += read_byte;

		// Close or EAGAIN or Error
		} else {

			// Interrupt other than SIGTERM, restart
			if (read_byte == -1 && errno == EINTR && !ptr->abort_flag_) {
				continue;
			}
			break;
		}
	}

	// Close socket connection
	if (read_byte == 0) {

		// Update alive mark
		pthread_mutex_lock(&ptr->alive_mutex_);
		ptr->is_alive_[jNode.fd] = false;
		pthread_mutex_unlock(&ptr->alive_mutex_);

		// Close socket
		if (close(jNode.fd) == -1) {
			perror("[Error] Close client socket failed");
		}
		fprintf(stderr, "[Info] Client %d is disconnected.\n", jNode.fd);

	// Read error
	} else if (read_byte == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		perror("[Error] Read error");

		/* Close socket */
		// Update alive mark
		pthread_mutex_lock(&ptr->alive_mutex_);
		ptr->is_alive_[jNode.fd] = false;
		pthread_mutex_unlock(&ptr->alive_mutex_);

		// Close socket
		if (close(jNode.fd) == -1) {
			perror("[Error] Close client socket failed");
		}
		fprintf(stderr, "[Info] Client %d is disconnected.\n", jNode.fd);

	// Read successfully
	} else {
		cur_content = str_buf;
		pthread_mutex_lock(&ptr->content_mutex_);

		// Store back
		ptr->cur_content_[jNode.fd] = cur_content;

		// Not yet finished
		if (cur_byte < need_byte) {
			ptr->cur_byte_[jNode.fd] = cur_byte;

		// Finished, change to WRITE mode
		} else {
			ptr->cur_status_[jNode.fd] = WRITE;
			ptr->cur_byte_[jNode.fd] = 0;
		}

		pthread_mutex_unlock(&ptr->content_mutex_);

		// Manually add write job into queue
		JobNode jTmp;
		jTmp.fd = jNode.fd;
		jTmp.events = EPOLLOUT;
		pthread_mutex_lock(&ptr->que_mutex_);
		ptr->client_que_.push(jTmp);
		pthread_cond_signal(&ptr->que_not_empty_);
		pthread_mutex_unlock(&ptr->que_mutex_);
	}
}

// Write string to client
void Server::WriteToClient(Server *ptr, const JobNode &jNode) {

	// Variable
	std::string cur_content = "";
	int cur_byte = 0;
	char str_buf[kBufSiz];

	// Retrieve current content of string (read/write)
	pthread_mutex_lock(&ptr->content_mutex_);
	cur_content = ptr->cur_content_[jNode.fd];
	cur_byte = ptr->cur_byte_[jNode.fd];
	pthread_mutex_unlock(&ptr->content_mutex_);

	// Copy to string
	memset(str_buf, 0, sizeof(str_buf));
	sprintf(str_buf, "%s", cur_content.c_str());

	// Write until byte reaches target
	int need_byte = sizeof(char) * kBufSiz;
	int write_byte = 0;
	while (cur_byte < need_byte) {
		write_byte = write(jNode.fd, str_buf + cur_byte, need_byte - cur_byte);

		// Update remaining
		if (write_byte > 0) {
			cur_byte += write_byte;

		// EAGAIN or Error
		} else {

			// Interrupt other than SIGTERM, restart
			if (write_byte == -1 && errno == EINTR && !ptr->abort_flag_) {
				continue;
			}
			break;
		}
	}

	// Write error
	if (write_byte == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		perror("[Error] Write error");

		/* Close socket */
		// Update alive mark
		pthread_mutex_lock(&ptr->alive_mutex_);
		ptr->is_alive_[jNode.fd] = false;
		pthread_mutex_unlock(&ptr->alive_mutex_);

		// Close socket
		if (close(jNode.fd) == -1) {
			perror("[Error] Close client socket failed");
		}
		fprintf(stderr, "[Info] Client %d is disconnected.\n", jNode.fd);

	// Write successfully
	} else {
		pthread_mutex_lock(&ptr->content_mutex_);

		// Not yet finished
		if (cur_byte < need_byte) {
			ptr->cur_byte_[jNode.fd] = cur_byte;

		// Finished, change to READ mode
		} else {
			ptr->cur_status_[jNode.fd] = READ;
			ptr->cur_content_[jNode.fd] = "";
			ptr->cur_byte_[jNode.fd] = 0;
		}

		pthread_mutex_unlock(&ptr->content_mutex_);
	}
}

// Destructor
Server::~Server(void) {

	// Clear up map and queue
	this->is_used_.clear();
	this->is_alive_.clear();
	while (!this->client_que_.empty()) this->client_que_.pop();

	// Destroy mutex and conditional variable
	pthread_mutex_destroy(&this->used_mutex_);
	pthread_mutex_destroy(&this->alive_mutex_);
	pthread_mutex_destroy(&this->que_mutex_);
	pthread_mutex_destroy(&this->content_mutex_);
	pthread_cond_destroy(&this->que_not_empty_);
}
