
#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {

	// Create socket
	Client client;
	if (!client.CreateSocket()) {
		exit(EXIT_FAILURE);
	}

	// Register signal
	if (!client.RegisterSignal()) {
		exit(EXIT_FAILURE);
	}

	// Connect to server
	if (!client.Connect()) {
		exit(EXIT_FAILURE);
	}

	// Wait for user to input string
	std::string input_string;
	std::cout << "(Send): ";
	std::getline(std::cin, input_string);

	// Send to server and receiver from server
	std::string output_string;
	if (!client.SendAndRecv(input_string, output_string)) {
		exit(EXIT_FAILURE);
	}
	std::cout << "(Recv): " << output_string << std::endl;

	// Close socket
	if (!client.CloseSocket()) {
		exit(EXIT_FAILURE);
	}

	return 0;
}
