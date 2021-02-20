/*
---------------------------------------------------------------------------------------------------------
	Project - "Server" process for managing Bulls and Cows games in a Gaming room between players
			  via client processes with multithreading & GameSession.txt Synchronized accessing
			  & Winsock API (Networking).

	Description - This is the server process that initiates the main thread that is meant to receive
		Clients connection requests, approve them (at most 3 at a time), create special Worker threads
		for each of them to handle communication (send\receive messages), manage a different thread
		that accepts the Server's User "EXIT" command if entered (from STDin) & evacuate all resources
		when termination is needed. 
		Overall, this process is a mediator that will handle the communication between different Clients
---------------------------------------------------------------------------------------------------------
*/

// Library includes -------------------------------------------------------------------------------------
#include <stdio.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")


// Projects includes ------------------------------------------------------------------------------------
//include every header
#include "HardCodedData.h"
#include "FetchAndValidateCommandlineArguments.h"
#include "SetCommunicationServerSide.h"

// Constants ----------------------------------------------------------------------------
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;


// Function Definition ------------------------------------------------------------------

int main(int argc, char* argv[]) {	
	int i = 0;// 0o0o0o0o0o  SERVER  0o0o0o0o0o
	unsigned short serverPortNumber = 0;
	//Validating the number of command line arguments
	if ((argc != 2) || (argv[1] == NULL)) {
		printf("Error: Incorrect number of arguments.\n");
		return 1;
	}


	/* ------------------------------------------------------------------------------------------------- */
	/*		Fetch the Server's port number from command line & Validate its' integrity:				     */
	/*							A number between 0 - 65536												 */
	/* ------------------------------------------------------------------------------------------------- */
	if (STATUS_CODE_FAILURE == fetchAndValidateCommandLineArguments(argv[1], &serverPortNumber, NULL, NULL)) return 1;

	












	/* --------------------------------------------------------------------------------------------------------------------------- */
	//The following function will perform all needed phases of the server process from opening a socket for listening, creating	   */
	//							Worker threads and operate incoming Clients connections										   	   */
	/* --------------------------------------------------------------------------------------------------------------------------- */
	if (STATUS_CODE_FAILURE == setCommmunicationServerSide(serverPortNumber)) {
		printf("Error: Failed to conduct waiting room & games for client processes (players).\n");
		return 1;
	}






	printf("\n\n\n\n...............................\n\nServer has finished properly!!!\n...............................\n\n\n\n\n\n");
	return 0;
}