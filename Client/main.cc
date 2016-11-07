
#include "client.h"

#include <stdio.h>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {

	// Create socket
	Client client;
	if (!client.CreateSocket()) return 1;

	// Connect to server
	if (!client.Connect()) return 1;

	// Wait for user to input string
	std::string input_string;
	std::cout << "(Send): ";
	std::getline(std::cin, input_string);
	
	// Send string to server
	if (!client.SendString(input_string)) return 1;

	// Receive string from server
	std::string output_string;
	if (!client.RecvString(output_string)) return 1;
	std::cout << "(Recv): " << output_string << std::endl;

	// Close socket
	if (!client.CloseSocket()) return 1;

	return 0;
}
