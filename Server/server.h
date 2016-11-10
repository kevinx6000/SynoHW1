#ifndef HW1_SERVER_SERVER_H_
#define HW1_SERVER_SERVER_H_

#define MAX_CLIENT 10002
#define MAX_EVENT 10010
#define kBufSiz 16383

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <map>

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
		static std::map<unsigned int, bool>is_alive_;
		static pthread_mutex_t map_mutex_;
};

#endif
