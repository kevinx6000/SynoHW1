#ifndef HW1_SERVER_SERVER_H_
#define HW1_SERVER_SERVER_H_

#define MAX_CLIENT 10002
#define MAX_EVENT 10010
#define MAX_THREAD 100
#define kBufSiz 16383

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <map>
#include <queue>

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
		static std::queue<int>client_que_;
		static pthread_mutex_t map_mutex_;
		static pthread_mutex_t que_mutex_;
		static pthread_cond_t que_not_empty_;
};

#endif
