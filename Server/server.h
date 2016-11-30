#ifndef HW1_SERVER_SERVER_H_
#define HW1_SERVER_SERVER_H_

#define MAX_CLIENT 20002
#define MAX_EVENT 20010
#define MAX_THREAD 1000
#define MAX_BUF 1023
#define MAX_FD 65536

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
		bool CloseAllSocket(void);
	
	private:
		// Job queue entry
		typedef struct {
			int fd;
			uint32_t events;
		}JobNode;

		// READ/WRITE status of client
		enum Status {
			READ,
			WRITE
		};

		// Content of client
		typedef struct {
			int byte;
			char *str;
			Status status;
		}Content;

	private:
		void Initialize(int);
		void ReadFromClient(int);
		void WriteToClient(int);
		bool MakeNonblocking(int);
		bool CloseClientSocket(int);

		static void *ServeClient(void *);

		// Server socket and port
		int port_;
		int server_socket_;

		// Abort flag (for SIGTERM)
		bool abort_flag_;

		// Thread pool
		pthread_t pid[MAX_THREAD];

		// Whether client fd is alive (or closed)
		pthread_mutex_t alive_mutex_;
		std::map<unsigned int, bool> is_alive_;

		// Whether client fd is served now
		pthread_mutex_t served_mutex_;
		std::map<unsigned int, bool> is_served_;

		// Client fd job queue
		pthread_mutex_t que_mutex_;
		std::queue<JobNode> client_que_;
		pthread_cond_t que_not_empty_;

		// Current content received from client fd
		Content content_[MAX_FD];
};

#endif
