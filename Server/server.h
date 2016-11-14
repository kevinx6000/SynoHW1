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
		Server(int);
		Server(const Server &);
		Server &operator=(const Server &);
		void Initialize(int);
		bool CreateSocket(void);
		bool AcceptConnection(void);
		bool EchoString(void);
		bool CloseSocket(void);
		~Server(void);
		static bool RegisterSignal(void);
		static void SignalHandler(int);
		static void *ServeClient(void *);
	private:
		int port_;
		int server_socket_;
		static bool is_sigterm_;
		std::map<unsigned int, bool> is_alive_;
		std::queue<int> client_que_;
		pthread_mutex_t map_mutex_;
		pthread_mutex_t que_mutex_;
		pthread_cond_t que_not_empty_;
		pthread_t pid[MAX_THREAD];
};

#endif
