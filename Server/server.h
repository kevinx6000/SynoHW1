#ifndef HW1_SERVER_SERVER_H_
#define HW1_SERVER_SERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

class Server {
	public:
		Server(void);
		bool CreateSocket(void);
		bool AcceptConnection(void);
		bool EchoString(void);
		bool CloseSocket(void);
		static void SignalHandler(int);
	private:
		static int server_socket_;
		static int client_socket_;
		int current_status_;
		struct sockaddr_in server_addr_;
		struct sockaddr_in client_addr_;
		const unsigned int kMaxConnection;
		const unsigned int kTokenLength;
};

#endif
