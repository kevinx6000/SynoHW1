#ifndef HW1_CLIENT_CLIENT_H_
#define HW1_CLIENT_CLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

class Client {
	public:
		Client()
			: kServerIP("10.13.21.117"), kTokenLength(100),
			kMaxStringLength((unsigned int)1e5) {}
		bool CreateSocket(void);
		bool Connect(void);
		bool SendString(std::string);
		bool RecvString(std::string &);
		bool CloseSocket(void);
	private:
		int client_socket_;
		struct sockaddr_in server_addr_;
		const char *kServerIP;
		const unsigned int kTokenLength;
		const unsigned int kMaxStringLength;
};

#endif
