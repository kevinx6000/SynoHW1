#include "server.h"

#include <stdio.h>

int main(int argc, char *argv[]) {

	// Create socket
	Server server;
	if (!server.CreateSocket()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		return -1;
	}

	// Register signal
	if (!server.RegisterSignal()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		return -1;
	}

	// Wait for connection
	if (!server.AcceptConnection()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		return -1;
	}

	// Close socket
	if (!server.CloseSocket()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		return -1;
	}
}
