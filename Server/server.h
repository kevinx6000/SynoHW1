#ifndef HW1_SERVER_SERVER_H_
#define HW1_SERVER_SERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

class Server {
	public:
		Server() : kMaxConnection(100), kTokenLength(100) {}
		bool CreateSocket(void);
		bool AcceptConnection(void);
		bool EchoString(void);
		bool CloseSocket(void);
	private:
		int server_socket_;
		int client_socket_;
		struct sockaddr_in server_addr_;
		struct sockaddr_in client_addr_;
		const unsigned int kMaxConnection;
		const unsigned int kTokenLength;
};

#endif
