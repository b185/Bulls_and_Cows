/* SetCommunicationClientSide.c
---------------------------------------------------------------------------------
	Module Description - This module contains functions meant for setting up the
		Client side connection by creating a socket, definning it, and attemting 
		to connect a server whose address was given as input to client.exe.
		It initiate a Client Speaker thread to handle that communication
---------------------------------------------------------------------------------
*/


//'CHECK'


// Library includes -------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")


// Projects includes -----------------------------------------------------------
#include "SetCommunicationClientSide.h"





// Constants
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;

static const BOOL GET_EXIT_CODE_FAILURE = 0;
static const BOOL LOOP_BACK_TO_CONNECT_SERVER = 2;

static const BOOL INETPTONS_SUCCESS = 1;
static const int SINGLE_OBJECT = 1;
static const int SINGLE_CHARACTER_ANSWER = 1;
//static const short DEFAULT_THREAD_STACK_SIZE = 0;

	

//0o0o0o Events initialization parameters values
static const BOOL MANUAL_RESET = TRUE;
static const BOOL AUTO_RESET = FALSE;
static const BOOL INITIALLY_SIGNALED = TRUE;
static const BOOL INITIALLY_NON_SIGNALED = FALSE;

static const BOOL INHERIT_HANDLE = TRUE;
static const BOOL DONT_INHERIT_HANDLE = FALSE;

//0o0o0o Server process closure status constants
//static const int KEEP_GOING = 0;
//static const BOOL STATUS_SERVER_ERROR = -1;
//static const BOOL STATUS_SERVER_EXIT = -2;





// Global variables ------------------------------------------------------------

//clientThreadPackage
clientThreadPackage* g_p_clientSpeakerThreadPackage = NULL;





// Functions declerations ------------------------------------------------------

/// <summary>
/// Description - This function creates a socket and a socket local address and updates the socket address with the inputted Server IP address and port number
/// </summary>
/// <param name="SOCKET** p_p_s_clientSocket - pointer address to a not yet allocated SOCKET"></param>
/// <param name="SOCKADDR_IN** p_p_service - pointer address to a not yet allocated SOCKADDR_IN struct"></param>
/// <param name="unsigned short serverPortNumber - input server port number"></param>
/// <param name="char* p_destinationServerIpAddress - input server ip address buffer"></param>
/// <returns>True if successful. False otherwise</returns>
static BOOL socketCreationAndDestinationSocketAddressCreation(SOCKET** p_p_s_clientSocket,
	SOCKADDR_IN** p_p_service, unsigned short serverPortNumber, char* p_destinationServerIpAddress);

/// <summary>
/// Description - creates a thread to begin the Client Speaker thread routine to communicate with the connected Server.
/// It also  creates a Client Speaker thread input package struct
/// </summary>
/// <param name="LPDWORD p_threadId - address to thread ID variable"></param>
/// <param name="char* p_playerNameString - User name buffer pointer"></param>
/// <param name="SOCKET* p_s_clientSocket - pointer to the socket"></param>
/// <returns>pointer to the handle to the newly initiated thread if successful, or NULL otherwise</returns>
static HANDLE* createAndInitiateThreadToCommunicateWithServer(LPDWORD p_threadId, char* p_playerNameString, SOCKET* p_s_clientSocket);
/// <summary>
/// Description - Allocates memory for a Client thread inputs package, and updates it
/// </summary>
/// <param name="char* p_playerNameString - User name buffer pointer to be inserted"></param>
/// <param name="SOCKET* p_s_clientSocket - pointer to a SOCKET "></param>
/// <returns>True if successful. False otherwise</returns>
static BOOL initiateSpeakerThreadPackages(char* p_playerNameString, SOCKET* p_s_clientSocket);

/// <summary>
/// Description - this function awaits the Client Speaker thread to complete its communication-with-server operation, then it validates its 
/// exit code, and frees the allocated memory and initiates a new socket for a re-attempt to connect the Server if the User desires.
/// </summary>
/// <param name="HANDLE* p_h_serverCommunicationThread - pointer to a thread Handle"></param>
/// <param name="char* p_ipAddressString - pointer to IP address buffer"></param>
/// <param name="unsigned short serverPortNumber - port number"></param>
/// <returns>True if successful. False otherwise</returns>
static BOOL intermediateThreadCheckUp(HANDLE* p_h_serverCommunicationThread, char* p_ipAddressString, unsigned short serverPortNumber);

/// <summary>
/// Description - This function initiates a reconnection attempts depended on the Speaker thread exit code:
/// SERVER_DENIED_COMM    SERVER_DISCONNECT   COMMUNICATION_TIMEOUT
/// </summary>
/// <returns>True if exiting successfuly, False if exiting with errors, 2 to loop back to reconnection </returns>
static BOOL reconnectionStdInputsocketRecreationAndThreadPackageRelease(/*will use the global g_p_clientSpeakerThreadPackage*/);

/// <summary>
/// Description - This function frees memory closes socket and terminates threads, and validates the operation checkup code
/// </summary>
/// <param name="HANDLE* p_h_serverCommunicationThread - pointer to a thread Handle"></param>
/// <param name="SOCKET* p_s_clientSocket - pointer to a SOCKET "></param>
/// <param name="SOCKADDR_IN* p_clientService - pointer to the socket's local address struct"></param>
/// <param name="int checkup - operation status code"></param>
/// <returns>updated operation status code</returns>
static BOOL communicationTerminationProcedure(HANDLE* p_h_serverCommunicationThread, SOCKET* p_s_clientSocket, SOCKADDR_IN* p_clientService, BOOL checkup);


// Functions definitions -------------------------------------------------------

BOOL setCommmunicationClientSide(char* p_ipAddressString, unsigned short serverPortNumber, char* p_playerNameString)
{
	//Winsock connectivity variables & pointers
	WSADATA wsaData;
	SOCKET* p_s_clientSocket = NULL;
	SOCKADDR_IN* p_clientService = NULL;
	//This variable will be filled with the User reponse to various interactions 
	int userStdInputNum = 0;
	//Threads pointers
	HANDLE* p_h_serverCommunicationThread = NULL;
	DWORD serverCommunicationThreadId = 0;
	BOOL checkupRes = 0;
	
	
	// Input integrity validation
	if ( (NULL == p_ipAddressString) || (NULL == p_playerNameString) ) {
		printf("Error: Bad inputs to function: %s\n", __func__); return STATUS_CODE_FAILURE;
	}





	// Initialize Winsock.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("Error: Failed to initalize Winsock API using WSAStartup( ) with error code no. %ld.\n Exiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}
	/* The WinSock DLL is acceptable. Proceed. */
	



	// Create a socket & create a DESTINATION socket address, comprised of a valid IP address (e.g. 127.0.0.1) and the input port number 
		//		associated with the Server the Client will attempt to connect to
		// Note: This function updates BOTH a SOCKET & a SOCKADDR_IN pointers and pointes them to an allocated & updated SOCKET & SOCKADDR_IN 
		//		 structs respectively. The SOCKET created is the Server's listening socket.
	if (STATUS_CODE_FAILURE == socketCreationAndDestinationSocketAddressCreation(&p_s_clientSocket, &p_clientService, serverPortNumber, p_ipAddressString)) {
		if (SOCKET_ERROR == WSACleanup())
			printf("Error: Failed to close Winsocket, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}



	
	while (TRUE) {
		// This is the "Connection loop".
		// Connection loop meant to interact with the 'player' which is the User of the Client process...
		// This loop will terminate once,
		//	1) User closes Client's window
		//	2) A connection with the Server has been made, and led to a service that has successfuly ended..
		//	3) A connection with the Server has seized, and the User will be led to decide whether to attempt reconnection or to leave
		//	4) A connection with the Server has failed to be established, and the User will be led to decide whether to attempt reconnection or to leave


		




		 
  
		// This is the "Failure Connection Menu".
		// In this menu, the User may choose to disconnect when experiencing connectivity failure, and the menu will be:
		//				Failed connecting to server on <ip> : <port>.
		//				Choose what to do next :
		//				1. Try to reconnect 
		//				2. Exit
		//				Type 1 or 2
	
		// Call the connect function, passing the created socket and the sockaddr_in structure as parameters. 
		// Check for general errors.
		if (SOCKET_ERROR == connect(*p_s_clientSocket, (SOCKADDR*)p_clientService, sizeof(*p_clientService))) {
			//Connection failed... printing menu to User....
			printf("Error: Failed to connect to 'Bulls & Cows' Gaming room Server, with error code no. %ld.\n", WSAGetLastError());
			printf("Failed connecting to server on %s : %hu.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\nType 1 or 2\n", p_ipAddressString, serverPortNumber);
			if (SINGLE_CHARACTER_ANSWER == scanf_s("%d", &userStdInputNum)) {
				if (1 == userStdInputNum) continue; //Reconnect attempt
				else {	
					//Disconnect - Close the client socket & free the socket and socket address structures' memory 
					//Set the output status result to Failure for communicationTerminationProcedure(.) function
					checkupRes = STATUS_CODE_FAILURE;
					break; //Exit Loop
					
				}
			}
			else{ //MIGHT CONVERT TO FUNCTION 'CHECK
				printf("Error: Failed to operate scanf_s(.) to read from STDin\n");
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				//Set the output status result to Failure for communicationTerminationProcedure(.) function
				checkupRes = STATUS_CODE_FAILURE;
				break; //Exit Loop
				
			}
		}

		//Connection to destination Server established.... 
		printf("\nConnected to server on %s:%hu\n\n", p_ipAddressString, serverPortNumber);
		/* --------------------------------------------------------------------------------------------------------------------- */
		/*...............Initiate Client Worker thread to operate the communication with the now connected Gaming Room Server	 */
		/* --------------------------------------------------------------------------------------------------------------------- */
		if (NULL == (p_h_serverCommunicationThread = createAndInitiateThreadToCommunicateWithServer(
			&serverCommunicationThreadId,			/* thread ID address */
			p_playerNameString,						/* one of the thread's inputs - User\Player's name */
			p_s_clientSocket))) {					/* another of the thread's inputs - Socket pointer */

			//Set the output status result to Failure for communicationTerminationProcedure(.) function
			checkupRes = STATUS_CODE_FAILURE;
			break; //Exit Loop
			
		}

		//Check the Client Speaker thread's status..
		//It decides whether a Timeout\Server disconnection\Server denied has occured, which returns the User to "Connection menu", 
		// or  whether a failure occured which terminates the Client  or  whether the Client (User) has chosen to disconnect...
		checkupRes = intermediateThreadCheckUp(p_h_serverCommunicationThread, p_ipAddressString, serverPortNumber);
		if ((STATUS_CODE_FAILURE == checkupRes) || (STATUS_CODE_SUCCESS == checkupRes)) break; //Exit "Connection menu" loop
		else continue; //Loop back to attempt re-connecting to Server
		

	}


	

	

	

	//Client process communicated with the Gaming room Server properly... Quit
	return communicationTerminationProcedure(p_h_serverCommunicationThread, p_s_clientSocket, p_clientService, checkupRes);
}












//......................................Static functions..........................................

static BOOL socketCreationAndDestinationSocketAddressCreation(SOCKET** p_p_s_clientSocket, 
	SOCKADDR_IN** p_p_service, unsigned short serverPortNumber, char* p_destinationServerIpAddress)
{
	SOCKET* p_clientSocket = NULL;
	SOCKADDR_IN* p_service = NULL;
	//Assert
	assert(NULL != p_p_s_clientSocket);
	assert(NULL != p_p_service);
	assert(NULL != p_destinationServerIpAddress);

	//Place SOCKET & SOCKADDR_IN pointers addresses in local pointers
	//p_clientSocket = *p_p_s_clientSocket; // might need to move both to be updated in the end and not in the beginning
	//p_service = *p_p_service;					  // might need to move 

	//SOCKET struct dynamic memory allocation
	if (NULL == (p_clientSocket = (SOCKET*)calloc(sizeof(SOCKET), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a SOCKET struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}

	//Create the Client socket
	if (INVALID_SOCKET == (*p_clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
		printf("Error: Failed to set a socket using socket( ), with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
		//Free the data of the Server's main(listening) socket & set its pointer's pointer to NULL
		free(p_clientSocket);
		return STATUS_CODE_FAILURE;
	}




	//SOCKADDR_IN struct dynamic memory allocation
	if (NULL == (p_service = (SOCKADDR_IN*)calloc(sizeof(SOCKADDR_IN), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a SOCKADDR_IN struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		//Close & free the data of the Server's main(listening) socket & set its pointer's pointer to NULL
		closeSocketProcedure(p_clientSocket); //also frees dynamic memory
		return STATUS_CODE_FAILURE;
	}

	/*
		Client applications use the IP address and port to connect to the host network.
		The sockaddr structure holds information regarding the address family, IP address, and port number.
		sockaddr_in is a subset of sockaddr and is used for IP version 4 applications.
   */


   //Update SOCKADDR_IN struct with the relevant IPv4 TCP connection information 
   /*
	   The three lines following the declaration of sockaddr_in service are used to set up
	   the sockaddr structure
   */

	//AF_INET is the Internet address family (IPv4 addresses family).
	p_service->sin_family = AF_INET;
	//"127.0.0.1" is the local IP address to which the socket might be bound.
	//  &p_service->sin_addr.s_addr in the address buffer pointer meant for the binary representation of the IP address
	//	NOTE: Instead of using inet_addr function e.g. p_service->sin_addr.s_addr = inet_addr(p_destinationServerIpAddress);   we use InetPton()
	if (INETPTONS_SUCCESS != InetPton(AF_INET, p_destinationServerIpAddress, &p_service->sin_addr.s_addr)) {
		printf("Error: Failed to translate the input Destination Server IP address to a binary value, with error code no. %d.\n", WSAGetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
		closeSocketProcedure(p_clientSocket); //also frees dynamic memory
		free(p_service);
		return STATUS_CODE_FAILURE;
	}
	//Input port number is the port number to which the socket will be bound.
	p_service->sin_port = htons(serverPortNumber); //The htons function converts a u_short from host to TCP/IP network byte order ( which is big-endian ).


	*p_p_s_clientSocket = p_clientSocket; // might need to move both to be updated in the end and not in the beginning
	*p_p_service = p_service;					  // might need to move 


	//Memory allocation for a SOCKET struct & socket Creation, Binding, Listening have been successful
	return STATUS_CODE_SUCCESS;
}




static HANDLE* createAndInitiateThreadToCommunicateWithServer(LPDWORD p_threadId, char* p_playerNameString, SOCKET* p_s_clientSocket)
{
	HANDLE* p_h_serverCommunicationThread = NULL;
	//clientThreadPackage* p_threadPackage = NULL;
	//Assert
	assert(NULL != p_threadId);

	//Allocate memory for a thread handle
	if (NULL == (p_h_serverCommunicationThread = (HANDLE*)calloc(sizeof(HANDLE), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for the Client's Communication-with-Server thread Handles (Client-side).\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
		return NULL;
	}

	//Construct the Client-side communication thread inputs parameters structure(package)
	if (STATUS_CODE_FAILURE == initiateSpeakerThreadPackages(p_playerNameString, p_s_clientSocket)) {
		free(p_h_serverCommunicationThread);
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
		return NULL;
	}

	/* ------------------------------------------------------------------------------------------------------------- */
	/*...............Initiate Client Worker thread to operate the communication with the Gaming Room Server			 */
	/* ------------------------------------------------------------------------------------------------------------- */
	if (INVALID_HANDLE_VALUE == (*p_h_serverCommunicationThread = createThreadSimple(
		(LPTHREAD_START_ROUTINE)clientSideSpeakerThreadRoutine,	/* the Client-side communication routine function address */
		g_p_clientSpeakerThreadPackage,										/* Client-side communication thread inputs package */
		p_threadId))) {											/* in the Client process there is a single thread which is the communication thread - this is its ID's address */

		
		printf("Error: Failed to initiate a thread for the Client to communicate with the Gaming Room Server, with error code no. %ld.\nExiting..\n\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
		//Free the thread's handle's dynamically allocated memory
		free(p_h_serverCommunicationThread);
		//free(p_threadPackage); 'CHECK'
		return NULL;
	}



	//Thread creation succeeded...
	return p_h_serverCommunicationThread;
}

//static BOOL openErrorEventsAndInsertTheirPointersToThreadPackages(char* p_playerNameString, SOCKET* p_s_clientSocket)
static BOOL initiateSpeakerThreadPackages(char* p_playerNameString, SOCKET* p_s_clientSocket)

{
	//Assert
	assert(NULL != p_playerNameString);
	assert(NULL != p_s_clientSocket);

	//clientThreadPackage struct dynamic memory allocation
	if (NULL == (g_p_clientSpeakerThreadPackage = (clientThreadPackage*)calloc(sizeof(clientThreadPackage), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a clientThreadPackage struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}
	

	//Update the Client thread package struct's fields with ALL the needed pointers 
	g_p_clientSpeakerThreadPackage->p_playerName = p_playerNameString;
	g_p_clientSpeakerThreadPackage->p_s_clientSocket = p_s_clientSocket;



	//Client thread inputs struct(package), the input for the thread in the Client that communicates with Server, was created successfuly
	//'CHECK'	with all needed Synchronous objects' pointers
	return STATUS_CODE_SUCCESS;
}







static BOOL intermediateThreadCheckUp(HANDLE* p_h_serverCommunicationThread, char* p_ipAddressString, unsigned short serverPortNumber)
{
	int waitErr = 0;
	BOOL retValGetExitCode = FALSE;
	DWORD exitCode = 0;
	//Assert
	assert(NULL != p_h_serverCommunicationThread);

	//Since the Client process invloves duration-limitless interaction with its User, the player, 
	// the main Client thread will wait indefinitely until either the Client User initiates a graceful disconnection,
	// OR  if any 'Error' occured at the Client-side (then the 'exit code' will show the failure) and the communication thread will exit as well,
	// OR  if the Server process   'Exited'\made an 'Error'   and terminates itself, so the Client will also terminate but with a successful  'exit code'
	switch (WaitForSingleObject(*p_h_serverCommunicationThread, INFINITE)) {
	case WAIT_OBJECT_0:
		//Thread terminated....
		break;

	case WAIT_TIMEOUT:
		//Timeout is irrelevant, because for any case the Client will either show the User a relevant menu  OR
		// just terminate the communication thread (NOT THE MAIN CLIENT THREAD!)
		waitErr = 1;
		break;

	default:
		printf("Error: Failed to wait for Client's Communication-with-Server thread to finish properly using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE; break;
	}


	//Forcefuly shutdown the Client's Communication-with-Server thread if it didn't finish(Due to INFINITE it is unlikely) !!!!!!!!
	if (1 == waitErr) {
		if (STATUS_CODE_FAILURE == TerminateThread(*p_h_serverCommunicationThread, 0x555/*arbitrary new exit code which isn't 'success'*/)) {
			printf("Error: Failed to terminateClient communication-with-Server thread, with error code no. %ld.\n\n", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		}
		return STATUS_CODE_FAILURE;
	}




	//Get the Client Speaker thread exit code
	retValGetExitCode = GetExitCodeThread(*p_h_serverCommunicationThread, &exitCode);
	//Validate the thread terminated correctly
	if (GET_EXIT_CODE_FAILURE == retValGetExitCode) { //add case for a thread which is STILL_ACTIVE
		printf("Error when getting the Client Speaker thread exit code, with code: %d\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}


	//Validate thread exit code....
	switch(exitCode){
	//Validate the thread terminated completely and no longer running
	case STILL_ACTIVE:
		printf("Client Speaker thread is still alive after threads termination timeout... exiting.\n");
		return STATUS_CODE_FAILURE; break;

	case (DWORD)COMMUNICATION_SUCCEEDED:
		//Validating the thread's return value(exit code) is as expected of a thread that completed its' operation successfuly...
		printf("Client Speaker thread has completed conducting the Server-Client messages transferral, after User decided to disconnect\n"); 
		return STATUS_CODE_SUCCESS; break;

	case (DWORD) GRACEFUL_DISCONNECT: // MIGHT BE COMM_SUCCEEDED
		//printf("Connection terminated gracefully\n");
		return STATUS_CODE_SUCCESS; break;

	case (DWORD)SERVER_DENIED_COMM:
		//Return to connection menu, because the Server denied the Client's connection attempt
		printf("Server on %s : %hu denied the connection request.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\nType 1 or 2\n", p_ipAddressString, serverPortNumber);
		return reconnectionStdInputsocketRecreationAndThreadPackageRelease(); break; //when User chooses to reconnect & socket re-definition is successful then returned value is LOOP_BACK_TO_CONNECT_SERVER

	case (DWORD)SERVER_DISCONNECTED:// 'CHANGE PRINTF' 
		printf("AFailed connecting to server on %s : %hu.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\nType 1 or 2\n", p_ipAddressString, serverPortNumber);
		return reconnectionStdInputsocketRecreationAndThreadPackageRelease(); break; //when User chooses to reconnect & socket re-definition is successful then returned value is LOOP_BACK_TO_CONNECT_SERVER

	case (DWORD)COMMUNICATION_TIMEOUT:// 'CHANGE PRINTF' 
		//printf("Client Speaker thread has reached timeout during the connection.. exiting\n");
		printf("BFailed connecting to server on %s : %hu.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\nType 1 or 2\n", p_ipAddressString, serverPortNumber);
		return reconnectionStdInputsocketRecreationAndThreadPackageRelease(); break; //when User chooses to reconnect & socket re-definition is successful then returned value is LOOP_BACK_TO_CONNECT_SERVER


	case (DWORD)COMMUNICATION_FAILED:
		printf("Exit code indicating a malfunction of the thread. Operation failed... exiting.\n");
		return STATUS_CODE_FAILURE; break;

	default:/*ignored*/
		printf("Default: not supposed to reach here.\n"); //even PLAYER_DISCONNECT which is in fact "SERVER_OPPONENT_QUIT" is handled within
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE; break;
	}
}

static BOOL reconnectionStdInputsocketRecreationAndThreadPackageRelease(/*will use the global g_p_clientSpeakerThreadPackage*/)
{
	int userStdInputNum = 0;
	//Assert
	assert(NULL != g_p_clientSpeakerThreadPackage);

	//Receive input from User (Input at STDin)
	if (SINGLE_CHARACTER_ANSWER != scanf_s("%d", &userStdInputNum)) {
		printf("Error: Failed to operate scanf_s(.) to read from STDin\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}

	//Input given by the Client User is '1'. Attempt to reconnect, by closing the existing socket & redefining it
	if (1 == userStdInputNum) {

		//Close the communication socket WITHOUT freeing the memory (Heap - dynamic) allocation of the socket's Handle.
		//First, perform an input integrity validation over the pointer of the socket 
		if (NULL != g_p_clientSpeakerThreadPackage->p_s_clientSocket) {
			//Attempting to close the given socket
			if (SOCKET_ERROR == closesocket(*(g_p_clientSpeakerThreadPackage->p_s_clientSocket))) {
				printf("Error: Failed close socket with code: %d.\n", WSAGetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n", __FILE__, __LINE__, __func__);
				return STATUS_CODE_FAILURE;
			}

			//Create the Client socket
			if (INVALID_SOCKET == (*(g_p_clientSpeakerThreadPackage->p_s_clientSocket) = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
				printf("Error: Failed to RE-set a socket using socket( ) for re-attempting connection, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
				//Free the data of the Server's main(listening) socket & set its pointer's pointer to NULL
				free(g_p_clientSpeakerThreadPackage->p_s_clientSocket);
				return STATUS_CODE_FAILURE;
			}

			//Since the "clientThreadPackage" data struct points to the same memory as the socket pointer in the setCommunicationClientSide(.),
			// and since initiateSpeakerThreadPackages(.) creates the thread input package, it is possible to free the  "clientThreadPackage" memory here
			// without losing a pointer to the new defined socket...
			free(g_p_clientSpeakerThreadPackage);
			g_p_clientSpeakerThreadPackage = NULL;
		}

		//Socket re-definition & "clientThreadPackage" release succeeded...
		return LOOP_BACK_TO_CONNECT_SERVER;
	}
	else
		// User chose to exit. Any one of the cases: 
		//		SERVER_DENIED_COMM
		//		SERVER_DISCONNECTED
		//		COMMUNICATION_TIMEOUT
		// led the Client program to this section, and all of these are considered CORRECT communication scenarios, thus we leave with a SUCCESS mark...
		return STATUS_CODE_SUCCESS;
}




static BOOL communicationTerminationProcedure(HANDLE* p_h_serverCommunicationThread, SOCKET* p_s_clientSocket, SOCKADDR_IN* p_clientService, BOOL checkup)
{
	

	
	//Close thread
	closeHandleProcedure(p_h_serverCommunicationThread);
	//Closing Socket & Freeing socket Handle Heap memory
	closeSocketProcedure(p_s_clientSocket);
	//Freeing Socket address structure
	free(p_clientService);
	//Freeint clientThreadPackage struct
	if(NULL != g_p_clientSpeakerThreadPackage) free(g_p_clientSpeakerThreadPackage);

	// Clean Winsock2
	if (SOCKET_ERROR == WSACleanup()) {
		printf("Error: Failed to close Winsocket, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}

	//Return the outcome of the exit code of the Client's communication thread...
	return (STATUS_CODE_SUCCESS == checkup) ? STATUS_CODE_SUCCESS : STATUS_CODE_FAILURE;
}







