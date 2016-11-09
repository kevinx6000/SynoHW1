#ifndef HW1_SERVER_SERVER_H_
#define HW1_SERVER_SERVER_H_

#define MAX_CLIENT 10002
#define kBufSiz 16383

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
		static void *ServeClient(void *);
	private:
		static int server_socket_;
		static int client_socket_[MAX_CLIENT];
		static int client_cnt_;
		int current_status_;
		struct sockaddr_in server_addr_;
		struct sockaddr_in client_addr_;
};

#endif
