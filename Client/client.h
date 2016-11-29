#ifndef HW1_CLIENT_CLIENT_H_
#define HW1_CLIENT_CLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

#define MAX_BUF 1023

class Client {
	public:
		Client(void);
		Client(const int);
		Client(const char *);
		Client(const char *, const int);
		Client(const Client &) = delete;
		Client &operator=(const Client &) = delete;
		~Client(void);

		bool CreateSocket(void);
		bool Connect(void);
		bool SendAndRecv(const std::string &, std::string &);
		bool CloseSocket(void);
		bool GetAbortFlag(void);
		void SetAbortFlag(bool);

	private:
		void Initialize(const char *, const int);
		bool SendString(const std::string &);
		bool RecvString(std::string &);

		int client_socket_;
		int server_port_;
		char *server_IP_;
		struct sockaddr_in server_addr_;
		bool is_abort_;
};

#endif
