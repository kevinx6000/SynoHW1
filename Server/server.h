#ifndef HW1_SERVER_SERVER_H_
#define HW1_SERVER_SERVER_H_

#define MAX_CLIENT 10002
#define MAX_EVENT 10010
#define MAX_THREAD 1000
#define kBufSiz 1023

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
		~Server(void);
		Server(const Server &) = delete;
		Server &operator=(const Server &) = delete;

		// Thread safety: No
		void SetAbortFlag(bool);

		bool CreateSocket(void);
		bool AcceptConnection(void);
		bool CloseSocket(void);

	private:
		void Initialize(int);

		static void *ServeClient(void *);

		int port_;
		int server_socket_;
		bool abort_flag_;
		std::map<unsigned int, bool> is_used_;
		std::map<unsigned int, bool> is_alive_;
		std::queue<int> client_que_;
		pthread_mutex_t used_mutex_;
		pthread_mutex_t alive_mutex_;
		pthread_mutex_t que_mutex_;
		pthread_cond_t que_not_empty_;
		pthread_t pid[MAX_THREAD];
};

#endif
