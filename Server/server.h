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
#include <string>

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
		// Job queue entry
		typedef struct {
			int fd;
			uint32_t events;
		}JobNode;

	private:
		void Initialize(int);
		void ReadFromClient(Server *, const JobNode &);
		void WriteToClient(Server *, const JobNode &);
		bool MakeNonblocking(int);

		static void *ServeClient(void *);

		int port_;
		int server_socket_;
		bool abort_flag_;

		// Whether client fd is alive (or closed)
		pthread_mutex_t alive_mutex_;
		std::map<unsigned int, bool> is_alive_;

		// Whether client fd is served now
		pthread_mutex_t used_mutex_;
		std::map<unsigned int, bool> is_used_;

		// Client fd job queue
		pthread_mutex_t que_mutex_;
		std::queue<JobNode> client_que_;
		pthread_cond_t que_not_empty_;

		// Current content received from client fd
		enum Status {READ, WRITE};
		pthread_mutex_t content_mutex_;
		std::map<unsigned int, std::string>cur_content_;
		std::map<unsigned int, Status>cur_status_;
		std::map<unsigned int, int>cur_byte_;

		pthread_t pid[MAX_THREAD];
};

#endif
