/*
---------------------------------------------------------------------------------------------------------
	Project - "Client" process for dicomposing numbers to their primal multipliers with multithreading

	Description - This is the client process that initiates the a Speaker thread to communicate with 
				  a designated Server, and through it communicate with other Clients and play
				  "Bulls & Cows" with these clients....
---------------------------------------------------------------------------------------------------------
*/

// Library includes -------------------------------------------------------------------------------------
#include <stdio.h>
#include <Windows.h>

// Projects includes ------------------------------------------------------------------------------------
//include every header
#include "FetchAndValidateCommandlineArguments.h"
#include "SetCommunicationClientSide.h"
#include "MessagesTransferringTools.h"
#include "ServerClientsTools.h"
#include "MemoryHandling.h"



// Constants ----------------------------------------------------------------------------
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;


// Function Definition ------------------------------------------------------------------

int main(int argc, char* argv[]) {
	unsigned short serverPortNumber = 0;
	int clientUserResponse = 0;
	char inputFromServerUser[] = "null";

	

	//Validating the number of command line arguments
	if ((argc != 4) || (argv[1] == NULL) || (argv[2] == NULL) || (argv[3] == NULL) ) {
		printf("Error: Incorrect number of arguments.\n");
		return 1;
	}


	/* ------------------------------------------------------------------------------------------------- */
	/*		Fetch the Server's port number from command line & Validate its' integrity:				     */
	/*							A number between 0 - 65536												 */
	/* ------------------------------------------------------------------------------------------------- */
	if (STATUS_CODE_FAILURE == fetchAndValidateCommandLineArguments(argv[2], &serverPortNumber, argv[1], argv[3])) return 1;


	
	
	

	
	

	/* --------------------------------------------------------------------------------------------------------------------------- */
	//The following function will perform all needed phases of the client process, from attempting to establish connection with	   */
	/* the Server by creating a socket a binding it to local address comprised of the analyzed inputs.							   */
	//	Also, it will execute the operation termination by freeing allocated memory and closing sockets.						   */
	/* --------------------------------------------------------------------------------------------------------------------------- */
	if (STATUS_CODE_FAILURE == setCommmunicationClientSide(argv[1], serverPortNumber, argv[3])) {
		printf("FINAL Error: Failed to communicate with designated Server properly.\n\n\n\n");
		return 1;
	}






	printf("Communication with designated Server was successful !!!!!\n\n\n\n\n\n");
	return 0;
}
