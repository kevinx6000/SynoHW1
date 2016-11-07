
#include "client.h"

#include <stdio.h>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {

	// Create socket
	Client client;
	if (!client.CreateSocket()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		return 1;
	}

	// Connect to server
	if (!client.Connect()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		return 1;
	}

	// Wait for user to input string
	std::string input_string;
	std::cout << "(Enter Sting): ";
	std::getline(std::cin, input_string);
	
	// Try to send to server
	if (client.SendString(input_string)) {
		
		// TODO: Receive the echo string
	}

	// Close socket
	if (!client.CloseSocket()) {
		fprintf(stderr, "[Error] Program terminated abnormally.\n");
		return 1;
	}

	return 0;
}
