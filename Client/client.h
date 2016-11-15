#ifndef HW1_CLIENT_CLIENT_H_
#define HW1_CLIENT_CLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

#define kBufSiz 1023

class Client {
	public:
		Client(void);
		Client(const int);
		Client(const char *);
		Client(const char *, const int);
		Client(const Client &);
		Client &operator=(const Client &);
		void Initialize(const char *, const int);
		bool CreateSocket(void);
		bool Connect(void);
		bool SendAndRecv(const std::string &, std::string &);
		bool CloseSocket(void);
		~Client(void);
		static bool RegisterSignal(void);
		static void SignalHandler(int);
		static bool GetIsSigterm(void);

	private:
		int client_socket_;
		int server_port_;
		char *server_IP_;
		struct sockaddr_in server_addr_;
		static bool is_sigterm_;

		// Only accessible to member functions
		bool SendString(const std::string);
		bool RecvString(std::string &);
};

#endif
