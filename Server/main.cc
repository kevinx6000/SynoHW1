#include "server.h"

#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

// Global variables
bool is_sigterm;

// Function prototype
bool RegisterSignal(void);
void SignalHandler(int);
void *ServerThread(void *);

int main(int argc, char *argv[]) {

	// Register signal
	is_sigterm = false;
	if (!RegisterSignal()) {
		fprintf(stderr, "[Error] Register signal failed\n");
		return -1;
	}
	
	// Create server thread
	Server server;
	pthread_t pid;
	if (pthread_create(&pid, NULL, ServerThread, (void *)&server) != 0) {
		perror("[Error] (main) Thread create failed");
		return -1;
	}

	// Wait on SIGTERM
	while (!is_sigterm) {
		sleep(3);
	}

	// Inform server SIGTERM
	fprintf(stderr, "[Info] SIGTERM received, waiting for server to terminate...\n");
	server.SetAbortFlag(true);

	// Wait for join
	pthread_join(pid, NULL);
}

// Register signal (SIGTERM)
bool RegisterSignal(void) {
	struct sigaction new_action;
	new_action.sa_handler = SignalHandler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	if (sigaction(SIGTERM, &new_action, NULL) < 0) {
		perror("Register signal failed");
		return false;
	}
	return true;
}

// Signal handler
void SignalHandler(int signum) {

	// Set SIGTERM mark and return
	is_sigterm = true;
}

// Server thread
void *ServerThread(void *para) {

	// Server object
	Server *server = (Server *)para;
	
	// Create socket
	if (!server->CreateSocket()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		pthread_exit(NULL);
	}
	
	// Wait for connection
	if (!server->AcceptConnection()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		pthread_exit(NULL);
	}

	// Close socket
	if (!server->CloseSocket()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		pthread_exit(NULL);
	}

	// Leave thread
	pthread_exit(NULL);
}
