
#include "client.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/epoll.h>

#include <iostream>
#include <string>

#define MAX_LEN 1023
#define MAX_EVENT 100

// Global variables
bool is_sigterm;

// Function prototype
bool RegisterSignal(void);
void SignalHandler(int);
void *ClientThread(void *);

int main(int argc, char *argv[]) {

	// Register signal
	is_sigterm = false;
	if (!RegisterSignal()) {
		fprintf(stderr, "[Error] Register signal failed\n");
		return -1;
	}

	// Create client thread
	Client client;
	pthread_t pid;
	if (pthread_create(&pid, NULL, ClientThread, (void *)&client) != 0) {
		perror("[Error] (main) Thread create failed");
		return -1;
	}

	// Wait on SIGTERM
	while (!is_sigterm) {
		sleep(3);
	}

	// Inform server SIGTERM
	fprintf(stderr, "\n[Info] SIGTERM received, waiting for client to terminate...\n");
	client.SetAbortFlag(true);

	// Wait for join
	pthread_join(pid, NULL);

	return 0;
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

// Client thread
void *ClientThread(void *para) {

	// Client object
	Client *client = (Client *)para;

	// Create socket
	if (!client->CreateSocket()) {
		pthread_exit(NULL);
	}

	// Connect to server
	if (!client->Connect()) {
		pthread_exit(NULL);
	}

	// Create epoll
	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		perror("[Error] epoll_create1");
		pthread_exit(NULL);
	}

	// Monitor stdin
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = STDIN_FILENO;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
		perror("[Error] epoll_ctl register failed");
		pthread_exit(NULL);
	}

	// Wait for user to input string
	int numFD = 1;
	char input_string[MAX_LEN];
	std::string output_string;
	struct epoll_event events[MAX_EVENT];
	while (!client->GetAbortFlag()) {

		// User input hint
		if (numFD > 0) {
			printf("(Send): ");
			fflush(stdout);
		}

		// Wait until stdin is ready, or abort
		numFD = epoll_wait(epoll_fd, events, MAX_EVENT, 3000);
		if (numFD == -1 && !client->GetAbortFlag()) {
			perror("[Error] epoll_wait");
			pthread_exit(NULL);
		}

		// stdin ready
		for (int i = 0; i < numFD && !client->GetAbortFlag(); i++) {

			// Read from stdin
			memset(input_string, 0, sizeof(char) * MAX_LEN);
			fgets(input_string, MAX_LEN, stdin);
	
			// Prune '\n'
			input_string[strlen(input_string)-1] = '\0';

			// Send to server and receiver from server
			if (!client->SendAndRecv(input_string, output_string)) {
				pthread_exit(NULL);
			}
			printf("(Recv): %s\n", output_string.c_str());
			fflush(stdout);
		}
	}

	// Close socket
	if (!client->CloseSocket()) {
		pthread_exit(NULL);
	}

	// Leave thread
	pthread_exit(NULL);
}
