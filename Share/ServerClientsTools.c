/* ServerClientsTools.c
--------------------------------------------------------------------------------------
	Module Description - This module contains functions meant to assist establishing 
		connection\disconnection& communication between a Server & Client, threads 
		handling
--------------------------------------------------------------------------------------
*/

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
#include "ServerClientsTools.h"

// Constants
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;


static const int SINGLE_OBJECT = 1;
static const short DEFAULT_THREAD_STACK_SIZE = 0;

//static const long MUTEX_WAIT_TIMEOUT_MILLI_SEC = 500; //0.5 Sec
static const BOOL MUTEX_RELEASE_FAILED = 0;

//0o0o0o WaitForMultipleObjects
static const BOOL  WAIT_FOR_ALL_OBJECTS = TRUE;

//0o0o0o Recived exit codes
static const BOOL  GET_EXIT_CODE_FAILURE = 0;

//0o0o0o Events initialization parameters values
static const BOOL MANUAL_RESET = TRUE;
static const BOOL AUTO_RESET = FALSE;
static const BOOL INITIALLY_SIGNALED = TRUE;
static const BOOL INITIALLY_NON_SIGNALED = FALSE;

static const int KEEP_RECEIVE_TIMEOUT = -1;


static const int GRACEFUL_DISCONNECT_WAITING_TIMEOUT = 2000;	//2 Seconds


// Functions declerations -------------------------------------------------------

/// <summary>
/// Description - calculates the length of a given message buffer received as input, while this buffer was constructed 
/// in such a way, that it ends with a Carriage Return byte, followed by a Line Feed byte, followed by a null teminating byte '\0'.
/// Note: message string construction was chosen this way to assist make the Writing\Reading to\from "GameSession.txt" file easier....
/// </summary>
/// <param name="const char* p_stringBuffer - pointer to buffer"></param>
/// <returns>size in bytes (integer)</returns>
static int fetchMessageStringLength(const char* p_stringBuffer);




/// <summary>
/// Description - sendString(.) is a wrapper that uses sendBuffer to send a complete buffer. It uses a socket to send a string.
/// </summary>
/// <param name="const char* p_messageToBeSent - pointer to the buffer containing the data in bytes to be sent"></param>
/// <param name="SOCKET s_socket - Socket Handle to send data through"></param>
/// <returns></returns>
static transferResults sendString(const char* p_messageToBeSent, SOCKET s_socket);


/// <summary>
/// Description - sendBuffer() uses a socket to send a buffer.
/// </summary>
/// <param name="const char* p_buffer - pointer to the buffer containing the data in bytes to be sent"></param>
/// <param name="int bytesToSend - number of bytes from the buffer to be sent"></param>
/// <param name="SOCKET s_socket - Socket Handle to send data through"></param>
/// <returns>TRANSFER_SUCCEEDED - if sending succeeded TRANSFER_FAILED - otherwise, which indicates a disconnection of the other side (Server\Client) will propagate to SERVER_DISCONNECTED</returns>
static transferResults sendBuffer(const char* p_buffer, int bytesToSend, SOCKET s_socket);


/// <summary>
/// Description - receiveString(.) is a wrapper that uses receiveBuffer to receive a complete buffer. It uses a socket to send a string.
/// </summary>
/// <param name="char** p_p_outputStringPointer - pointer address to that will evantually point at the newly memory allocated & updated buffer containing the data read"></param>
/// <param name="SOCKET s_socket - Socket Handle to receive data from"></param>
/// <returns>TRANSFER_SUCCEEDED - if receiving succeeded ; TRANSFER_DISCONNECTED - if the socket was disconnected gracefully ; TRANSFER_TIMEOUT - if recv(.) timeout ; TRANSFER_FAILED - if Server\Client disconnected abruptly</returns>
static transferResults receiveString(char** p_p_outputStringPointer, SOCKET s_socket);


/// <summary>
///  Description - receiveBuffer() uses a socket to receive a buffer.
/// </summary>
/// <param name="char* p_outputBuffer - pointer to a pre-mem-allocated buffer to which the data will be read"></param>
/// <param name="int bytesToReceive - size in bytes of output buffer"></param>
/// <param name="SOCKET s_socket - Socket Handle to receive data through"></param>
/// <returns>TRANSFER_SUCCEEDED - if receiving succeeded ; TRANSFER_DISCONNECTED - if the socket was disconnected gracefully ; TRANSFER_TIMEOUT - if recv(.) timeout ; TRANSFER_FAILED - if Server\Client disconnected abruptly</returns>
static transferResults receiveBuffer(char* p_outputBuffer, int bytesToReceive, SOCKET s_socket);


// Functions definitions -------------------------------------------------------


HANDLE createThreadSimple(LPTHREAD_START_ROUTINE p_startRoutine, LPVOID p_threadParameters, LPDWORD p_threadId)
{
	HANDLE h_threadHandle;

	//Asserts
	assert(NULL != p_startRoutine);
	assert(NULL != p_threadId);


	//Thread creation
	h_threadHandle = CreateThread(
		NULL,										 /*  default security attributes - no need that the thread can be inherited by child processes */
		DEFAULT_THREAD_STACK_SIZE,					 /*  use default stack size - most of the objects (e.g. section struct) are allocated in the heap instead */
		p_startRoutine,								 /*  thread function - remember it is an address! */
		p_threadParameters,							 /*  argument to thread function - this will be a pointer to the socket yielded from accepting a Client's connection */
		0,											 /*  use default creation flags - thread runs immediately when it is created */
		p_threadId);								 /*  returns the thread identifier - thread ID */

	//Validate the thread creation was successful
	if (NULL == h_threadHandle) {
		printf("Error: Failed to initiate a thread for a newly connected Client, with error code no. %ld.\nExiting..\n\n", GetLastError());
		return INVALID_HANDLE_VALUE;
	}

	//Returning handle
	return h_threadHandle;
}

HANDLE* allocateMemoryForHandleAndCreateEvent(BOOL eventManualAutoReset, BOOL eventInitialState, LPCSTR p_eventName)
{
	HANDLE* p_h_eventHandle = NULL;
	//Assert
	assert((MANUAL_RESET == eventManualAutoReset) || (AUTO_RESET == eventManualAutoReset));
	assert((INITIALLY_SIGNALED == eventInitialState) || (INITIALLY_NON_SIGNALED == eventInitialState));

	//Allocating dynamic memory (Heap) for a Event Handle 
	if (NULL == (p_h_eventHandle = (HANDLE*)calloc(sizeof(HANDLE), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for an Event synchronous object.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return NULL;
	}

	//Creating an un-named\named Event (It is sufficent for it to be un-named, because the pointer to the handle will be transfered to all accessing threads)
	*(p_h_eventHandle) = CreateEvent(
		NULL,						/* default security attributes */
		eventManualAutoReset,		/* manual\auto-reset event according to input */
		eventInitialState,			/* initial state is signaled\non-signaled according to input */
		p_eventName);				/* may be named. if null is given, then un-named */
	//Validate the Event creation (The Event synchronous object is created un-named so there is no need to check ERROR_ALREADY_EXISTS case) 
	if (NULL == *(p_h_eventHandle)) {
		printf("Error: Failed to create a Handle to an Event with code: %ld.\n", GetLastError());
		free(p_h_eventHandle);
		return  NULL;
	}
	//named case...


	//Event creation was succesful
	return p_h_eventHandle;
}


/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

int incrementDecrementCheckNumberOfConnectedClients(USHORT* p_currentNumOfConnectedClients, HANDLE* p_h_connectedClientsNumMutex, int increDecreVal, long timeout)
{
	int result = 0;
	//Input integrity validation
	if ((NULL == p_currentNumOfConnectedClients) || (NULL == p_h_connectedClientsNumMutex) || (!((-1 <= increDecreVal) && (1 >= increDecreVal))) ) {
		printf("Error: Bad inputs to function: %s\n", __func__); return STATUS_CODE_FAILURE;
	}

	
	//Attemmpt to OWN the Number of currently connected Clients resource's Mutex
	if (WAIT_OBJECT_0 != WaitForSingleObject(*p_h_connectedClientsNumMutex, timeout)) {
		return -1;
	}
	//Mutex owned... execute incrementation\decrementation 
	*p_currentNumOfConnectedClients += increDecreVal;
	//else
		//Special Case when the main Server thread calls this function after accepting the connection of a Third Client (in this specific time)
		//	thus, the Client will be rejected from the Game Room. For the scenario in which a third player was approved connection and two players
		//	have finished their game in this very moment, and one player chooses to disconnect while the other choose to stay for another game, 
		//	the third player will still be kicked.... becuase it connected while there were still two players in game!
	result = *p_currentNumOfConnectedClients;

	//Release the ownership over the resource Mutex
	if (MUTEX_RELEASE_FAILED == ReleaseMutex(*p_h_connectedClientsNumMutex)) {
		printf("Error: Thread no. %lu failed to release the Number of currently connected Clients Mutex, with code: %d.\nExiting...\n", GetCurrentThreadId(), GetLastError());
		return -1;
	}

	//"Writing"\"Reading" to\from the Number of currently connected Clients resource succeeded  
	return result; 
	// current number of connected clients CANNOT go below 0!!!! because only Worker threads decrement and only main thread increment 
	// and a main thread will always increment before IT creates a Worker thread that will evantually decrement.
}

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/


int validateThreadsWaitCode(HANDLE* p_h_threadHandles, int numberOfThreads, int timeout)
{
	DWORD waitCode = 0;
	//Asserts
	assert(NULL != p_h_threadHandles);
	assert(0 <= numberOfThreads);
	//Special case: No Clients entered at all!
	//if (0 == numberOfThreads) return STATUS_CODE_SUCCESS;

	//Wait for 3 seconds for all threads to finish...
	waitCode = WaitForMultipleObjects(
		numberOfThreads,			// number of objects in array
		p_h_threadHandles,			// array of objects
		WAIT_FOR_ALL_OBJECTS,		// wait for any object
		timeout);					// timeout in milli-seconds

	//Validating that all threads have finished....
	switch (waitCode) {
	case WAIT_OBJECT_0:
		printf("\nAll threads terminated on time.\nProceed to validate correctness of the exit codes of the threads...\n"); return WAIT_OBJECT_0;
	case WAIT_TIMEOUT:
		printf("\nTIMEOUT: Not all threads terminated on time... \nClients may still be connected to Server's Worker threads.\n"); return WAIT_TIMEOUT;
	case WAIT_ABANDONED_0:
		printf("\nAt least one of the objects is an abandoned Mutex object... \nEXITING....\n"); return WAIT_ABANDONED_0;
	default: // 'CHECK' JUMPS HERE AFTER 3 CLIENTS CONNECTED & DISCONNECTED IN DIFFERENT TIME INTERVALS
		printf("0x%x\nWaitForMultipleObjects(.) failed. Extended error code: %d\n", waitCode, GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return WAIT_FAILED;
	}
}

BOOL validateThreadExitCode(HANDLE* p_h_threadHandle/*, int numberOfThreads*/)
{
	BOOL retValGetExitCode = FALSE;
	DWORD exitCode = 0;
	//Asserts
	assert(NULL != p_h_threadHandle);
	//assert(0 <= numberOfThreads);
	//Special case: No Clients entered at all!
	//if (0 == numberOfThreads) return STATUS_CODE_SUCCESS;

	//printf("%d\n\n\n", numberOfThreads);

	
	//Fetch the exit code & the GetExitCodeThread function return value of the t'th thread
	if(NULL != *p_h_threadHandle ) //findFirstUnusedThreadSlot(.) may have changed the Handle's value to NULL, or it was NULL to begin with (e.g. only two players were connected at every single time)
		retValGetExitCode = GetExitCodeThread(*p_h_threadHandle, &exitCode);

	//Validate the thread terminated correctly
	if (GET_EXIT_CODE_FAILURE == retValGetExitCode) { //add case for a thread which is STILL_ACTIVE
		printf("Error when getting thread no. CHANGE exit code, with code: %d\n",  GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}

	//Validating the thread's return value(exit code) is as expected of a thread that completed its' operation successfuly...
	//There are several exit codes, and each may demand a different handling
	switch (exitCode) {
		//Validate the thread terminated completely and no longer running
	case STILL_ACTIVE:
		printf("Thread no. change is still alive after threads termination timeout... exiting.\n");
		return STATUS_CODE_FAILURE;

	case(DWORD)COMMUNICATION_FAILED:
		printf("Exit code indicating a malfunction of the thread. Operation failed... exiting.\n");
		return STATUS_CODE_FAILURE;

	case(DWORD)COMMUNICATION_EXIT:
		//Other thread in the Server process failed
		return STATUS_CODE_SUCCESS;

	case(DWORD)COMMUNICATION_TIMEOUT:
		//The connection between the current Server Worker thread and a Client timedout for some recv(.) operation... System resumes
		return STATUS_CODE_SUCCESS;

	case(DWORD)SERVER_DISCONNECTED:
		//The connection between the current Server Worker thread and a Client abruptly disconnected (failure of recv(.)\send(.))... System resumes
		return STATUS_CODE_SUCCESS;

	case(DWORD)PLAYER_DISCONNECTED:
		//Player chose to disconnect with message "CLIENT_DISCONNECT" at Server's main menu phase... System resumes
		return STATUS_CODE_SUCCESS;

	case(DWORD)SERVER_DENIED_COMM:
		//Server chose to deny the connection with the Client with message "SERVER_DENIED" because Client was a 3rd Player... System resumes
		return STATUS_CODE_SUCCESS;

	case(DWORD)GRACEFUL_DISCONNECT:
		//The connection between the current Server Worker thread and a Client gracefully disconnected... System resumes
		return STATUS_CODE_SUCCESS;

	default: //COMMUNICATION_SUCCEEDED...... back to menu shouldn't return
		//Thread terminated & finished conducting communication successfuly...
		return STATUS_CODE_SUCCESS;
	}
}


/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/


static transferResults sendBuffer(const char* p_buffer, int bytesToSend, SOCKET s_socket)
{
	const char* p_currentPointerPosition = p_buffer;
	int bytesTransferred = 0, remainingBytesToSend = bytesToSend;
	//Assert
	assert(NULL != p_buffer);
	assert(0 <= bytesToSend);
	assert(INVALID_SOCKET != s_socket);

	//Begin main sending loop... loop halts when all bytes are sent, or if an error occurs
	while (remainingBytesToSend > 0)
	{
		//Send does not guarantee that the entire message is sent 
		bytesTransferred = send(s_socket, p_currentPointerPosition, remainingBytesToSend, 0 /* no flags */);
		if (bytesTransferred == SOCKET_ERROR){
			printf("Error: send() function failed, with error code no. %ld.\n", WSAGetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			return TRANSFER_FAILED;
		}

		//Update #Bytes remaining to send & the Buffer's pointer current position
		remainingBytesToSend -= bytesTransferred;
		p_currentPointerPosition += bytesTransferred; // <ISP> pointer arithmetic
	}

	//Transfer operation (send) was successful....
	return TRANSFER_SUCCEEDED;
}

static transferResults sendString(const char* p_messageToBeSent, SOCKET s_socket)
{
	int totalStringSizeInBytes = 0;
	transferResults sendResult = TRANSFER_FAILED;
	//Input integrity validation
	if ((NULL == p_messageToBeSent) || (INVALID_SOCKET == s_socket)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return TRANSFER_FAILED;
	}

	/* Sending protocol is agreed to divide the message into two parts & send every part individually:
	 First the Length of the string (stored in an int variable), then the string itself. */ 

	//Calculate the message's length
	totalStringSizeInBytes = fetchMessageStringLength(p_messageToBeSent) + 1; // terminating zero also sent	

	//First, send the message's length 
	sendResult = sendBuffer(
		(const char*)(&totalStringSizeInBytes),		/* a pointer to the buffer containing the data representing the value of the message buffer size */
		(int)(sizeof(totalStringSizeInBytes)),		/* the size of the value within the buffer of message buffer length, is the size of int -sizeof(int) */ 
		s_socket);									/* socket used to send data */
	//If sending the message's length, it is okay to stop here and return a Failure result...
	if (sendResult != TRANSFER_SUCCEEDED) return sendResult;

	//Second, send the message itself 
	sendResult = sendBuffer(
		(const char*)(p_messageToBeSent),			/* a pointer to the message buffer data */
		(int)(totalStringSizeInBytes),				/* the message buffer length received in the previous receive interval */ 
		s_socket);									/* socket used to send data */

	//Return the sending operation result
	return sendResult;
}

//OVERALL - RETURNED VALUES MAY BE: TRANSFER_FAILED, TRANSFER_SUCCEEDED, TRANSFER_PREVENTED -> SERVER_DISCONNECTED, COMMUNICATION_SUCCEEDED, COMMUNICATION_FAILED
communicationResults sendMessageClientSide(SOCKET* p_s_clientCommunicationSocket, int messageTypeSerialNumber, char* p_paramOne/*, int timeoutForNextResponse*/)
{
	messageString* p_messageOrResponseToServer = NULL;
	//int setClientSocketReceiveTimeoutResult = 0, socketReceiveFromServerTimeoutDuration = 0;
	//Input integrity validation
	if ((NULL == p_s_clientCommunicationSocket) || (CLIENT_DISCONNECT_NUM < messageTypeSerialNumber) || (SERVER_MAIN_MENU_NUM > messageTypeSerialNumber)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return COMMUNICATION_FAILED; //parameters pointer may be NULL
	}

	//Construct the output message buffer & Send to Server
	if (NULL != (p_messageOrResponseToServer = constructMessageForSendingClient(messageTypeSerialNumber, p_paramOne))) {
		if (TRANSFER_SUCCEEDED != sendString(p_messageOrResponseToServer->p_messageBuffer, *p_s_clientCommunicationSocket)) {
			printf("Error: Failed to send message whose type is %d from Client to Server.\n", messageTypeSerialNumber); //'CHECK' maybe somehow change to message type string (maybe print outside)
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			freeTheString(p_messageOrResponseToServer);
			
			return (communicationResults)TRANSFER_FAILED; 
		}
	}
	else {
		//Message construction failed, and is printed within the function  == TRANSFER_PREVENTED
		//FATAL ERROR - attempt "Graceful Disconnect"
		gracefulDisconnect(p_s_clientCommunicationSocket);
		return (communicationResults)TRANSFER_PREVENTED;
	}

	/*
	//If the input timeout is greater than -1, then timeout duration on 'receive' operation will be altered
	if (KEEP_RECEIVE_TIMEOUT != timeoutForNextResponse) {
		//Adjust the socket timeout definitions using setsockopt(.) function....
		socketReceiveFromServerTimeoutDuration = timeoutForNextResponse; //Timeout duration in milli-seconds
		setClientSocketReceiveTimeoutResult = setsockopt(
			*p_s_clientCommunicationSocket,						/ * socket of either the Client Speaker t.(that communicates with a Server Worker t.) or the Server Worker t.(that communicates with a Client Speaker t.) * /
		//	SOL_SOCKET,											/ * the level of the socket in which we set an option's definition - this is the "socket" layer\level * /
		//	SO_RCVTIMEO,										/ * the option that will be set in the operation - this is the receive timeout duration option * /
		//	(void*)&socketReceiveFromServerTimeoutDuration,		/ * a void type  pointer  to the BUFFER containing bytes of data representing the new value that will be set to the socket's option * /
		//	sizeof(socketReceiveFromServerTimeoutDuration));	/ * the  size  of the data contained in the BUFFER * /
		
		//Validate setsockopt(.) operation result
		if (SOCKET_ERROR == setClientSocketReceiveTimeoutResult) {
			printf("Error: Failed to alter the socket's 'receive' timeout duration, with error code no. %d.\n", WSAGetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			freeTheString(p_messageOrResponseToServer);
			return TRANSFER_FAILED;
		}
	}*/


	//Free the buffer struct used to send the message to the Server
	freeTheString(p_messageOrResponseToServer);
	//Sending was successful.....
	return (communicationResults)TRANSFER_SUCCEEDED;
}

communicationResults sendMessageServerSide(SOCKET* p_s_serverCommunicationSocket, int messageTypeSerialNumber,
	char* p_paramOne, char* p_paramTwo, char* p_paramThree, char* p_paramFour/*, int timeoutForNextResponse*/)
{
	messageString* p_messageOrResponseToClient = NULL;
	//int setServerWorkerSocketReceiveTimeoutResult = 0, socketReceiveFromClientTimeoutDuration = 0;
	//Input integrity validation
	if ((NULL == p_s_serverCommunicationSocket) || (CLIENT_DISCONNECT_NUM < messageTypeSerialNumber) || (SERVER_MAIN_MENU_NUM > messageTypeSerialNumber)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return COMMUNICATION_FAILED; //parameters pointer may be NULL
	}

	//Construct the output message buffer & Send to Client
	if (NULL != (p_messageOrResponseToClient = constructMessageForSendingServer(messageTypeSerialNumber, p_paramOne, p_paramTwo, p_paramThree, p_paramFour))) {
		if (TRANSFER_SUCCEEDED != sendString(p_messageOrResponseToClient->p_messageBuffer, *p_s_serverCommunicationSocket)) {
			printf("Error: Failed to send message whose type is %d from Client to Server.\n", messageTypeSerialNumber); //'CHECK' maybe somehow change to message type string (maybe print outside)
			freeTheString(p_messageOrResponseToClient);
			
			return (communicationResults)TRANSFER_FAILED;
		}
	}
	else {
		//Message construction failed, and is printed within the function  == TRANSFER_PREVENTED
		//FATAL ERROR - attempt "Graceful Disconnect"
		gracefulDisconnect(p_s_serverCommunicationSocket);
		return (communicationResults)TRANSFER_PREVENTED;
	}

	/*	//If the input timeout is greater than -1, then timeout duration on 'receive' operation will be altered
	if (KEEP_RECEIVE_TIMEOUT != timeoutForNextResponse) {
		//Adjust the socket timeout definitions using setsockopt(.) function....
		socketReceiveFromClientTimeoutDuration = timeoutForNextResponse; //Timeout duration in milli-seconds
		setServerWorkerSocketReceiveTimeoutResult = setsockopt(
			*p_s_serverCommunicationSocket,						/ * socket of either the Client Speaker t.(that communicates with a Server Worker t.) or the Server Worker t.(that communicates with a Client Speaker t.) * /
		//	SOL_SOCKET,											/ * the level of the socket in which we set an option's definition - this is the "socket" layer\level * /
		//	SO_RCVTIMEO,										/ * the option that will be set in the operation - this is the receive timeout duration option * /
		//	(void*)&socketReceiveFromClientTimeoutDuration,		/ * a void type  pointer  to the BUFFER containing bytes of data representing the new value that will be set to the socket's option * /
		//	sizeof(socketReceiveFromClientTimeoutDuration));	/ * the  size  of the data contained in the BUFFER * /

		//Validate setsockopt(.) operation result
		if (SOCKET_ERROR == setServerWorkerSocketReceiveTimeoutResult) {
			printf("Error: Failed to alter the socket's 'receive' timeout duration, with error code no. %d.\n", WSAGetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			freeTheString(p_messageOrResponseToClient);
			return TRANSFER_FAILED;
		}
	}*/


	//Free the buffer struct used to send the message to the Server
	freeTheString(p_messageOrResponseToClient);
	//Sending was successful.....
	return (communicationResults)TRANSFER_SUCCEEDED;
}
/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

static transferResults receiveBuffer(char* p_outputBuffer, int bytesToReceive, SOCKET s_socket)
{
	char* p_currentPointerPosition = p_outputBuffer;
	int bytesJustTransferred = 0, remainingBytesToReceive = bytesToReceive;
	//Assert
	assert(NULL != p_outputBuffer);
	assert(0 <= bytesToReceive);
	assert(INVALID_SOCKET != s_socket);

	//Begin main receiving loop... loop halts when all bytes are received, or if an error occurs
	while (remainingBytesToReceive > 0)
	{
		/* send does not guarantee that the entire message is sent */
		bytesJustTransferred = recv(s_socket, p_currentPointerPosition, remainingBytesToReceive, 0 /* no flags */);
		if (bytesJustTransferred == SOCKET_ERROR) {  
			//SOCKET_ERROR may indicate the 'receive' function has reached its predetermined timeout..
			if (WSAGetLastError() == WSAETIMEDOUT) {
				printf("recv(.) function's running time duration reached its timeout. Exiting\n");
				return TRANSFER_TIMEOUT; //'CHECK' add printf for every message that cause timeout
			}
			//else, the 'receive' function failed completely...
			printf("Error: recv() failed, with error code %ld.\n", WSAGetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			return TRANSFER_FAILED;
		}
		//The communicating Server & Client disconnected "Gracefuly"
		else if (bytesJustTransferred == 0)
			return TRANSFER_DISCONNECTED; // recv() returns zero if connection was gracefully disconnected.

		//Update #Bytes remaining to receive & the Buffer's pointer current position
		remainingBytesToReceive -= bytesJustTransferred;
		p_currentPointerPosition += bytesJustTransferred; // <ISP> pointer arithmetic
	}

	//Transfer operation (recv) was successful....
	return TRANSFER_SUCCEEDED;
}

static transferResults receiveString(char** p_p_outputStringPointer, SOCKET s_socket)
{
	int totalStringSizeInBytes = 0;
	transferResults receiveResult = TRANSFER_FAILED;
	char* p_messageBuffer = NULL;
	//Input integrity validation
	if ((NULL == p_p_outputStringPointer) || (*p_p_outputStringPointer != NULL) || (INVALID_SOCKET == s_socket)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return TRANSFER_FAILED;
	}

	/*
	if ((p_p_outputStringPointer == NULL) || (*p_p_outputStringPointer != NULL))
	{
		printf("The first input to ReceiveString() must be "
			"a pointer to a char pointer that is initialized to NULL. For example:\n"
			"\tchar* Buffer = NULL;\n"
			"\tReceiveString( &Buffer, ___ )\n");
		return TRANSFER_FAILED;
	}*/

	/* Receving protocol is agreed to divide the message into two parts:
	   First the Length of the string (stored in an int variable), then the string itself. */

	//First, receive the message's length 
	receiveResult = receiveBuffer(
		(char*)(&totalStringSizeInBytes),
		(int)(sizeof(totalStringSizeInBytes)), // 4 bytes
		s_socket);

	//If receving the message's length, it is okay to stop here and return a Failure result...			// May return either TRANSFER_FAILED\TRANSFER_DISCONNECTED
	if (receiveResult != TRANSFER_SUCCEEDED) return receiveResult;			


	//Allocate dynamic memory for the incoming message
	if (NULL == (p_messageBuffer = (char*)malloc(totalStringSizeInBytes * sizeof(char)))) {
		printf("Error: Failed to allocate memory for the received message string.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return TRANSFER_FAILED;
	}
	
	//Second, receive the message itself 
	receiveResult = receiveBuffer(
		(char*)(p_messageBuffer),
		(int)(totalStringSizeInBytes),
		s_socket);

	if (receiveResult == TRANSFER_SUCCEEDED) *p_p_outputStringPointer = p_messageBuffer;
	else 
		free(p_messageBuffer);

	//If the transfer was successful then p_p_outputStringPointer points at the message's pointer's address & receiveResult is set to TRANSFER_SUCCEEDED
	// else the allocated memory for the message's buffer was freed & receiveResult is set to TRANSFER_FAILED
	return receiveResult; // May return either TRANSFER_SUCCEEDED\TRANSFER_FAILED\TRANSFER_DISCONNECTED
}

transferResults receiveMessage(SOCKET* p_s_communicationSocket, message** p_p_receivedMessageInfo, int responseReceiveTimeoutValue)
{
	int receiveResult = 0, setClientSocketReceiveTimeoutResult = 0, socketReceiveFromServerTimeoutDuration = 0;// , selectRes = 0;
	char* p_receivedMessageBuffer = NULL;
	//fd_set set;
	//struct timeval timeout;

	//Input integrity validation
	if ((NULL == p_s_communicationSocket) || (NULL == p_p_receivedMessageInfo)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return TRANSFER_FAILED; //parameters pointer may be NULL
	}




	//Implement the receive timeout altering duration!
	//If the input timeout is greater than -1, then timeout duration on 'receive' operation will be altered
	if (KEEP_RECEIVE_TIMEOUT != responseReceiveTimeoutValue) {
		//Adjust the socket timeout definitions using setsockopt(.) function....
		socketReceiveFromServerTimeoutDuration = responseReceiveTimeoutValue; //Timeout duration in milli-seconds
		setClientSocketReceiveTimeoutResult = setsockopt(
			*p_s_communicationSocket,							/* socket of either the Client Speaker t.(that communicates with a Server Worker t.) or the Server Worker t.(that communicates with a Client Speaker t.) */
			SOL_SOCKET,											/* the level of the socket in which we set an option's definition - this is the "socket" layer\level */
			SO_RCVTIMEO,										/* the option that will be set in the operation - this is the receive timeout duration option */
			(void*)&socketReceiveFromServerTimeoutDuration,		/* a void type  pointer  to the BUFFER containing bytes of data representing the new value that will be set to the socket's option */
			sizeof(socketReceiveFromServerTimeoutDuration));	/* the  size  of the data contained in the BUFFER */

		//printf("setsocketopt  %d\n\n\n", setClientSocketReceiveTimeoutResult); 'DELETE'
		//TEMP
		///printf("\n\n\n\n\nresults %d\n", setClientSocketReceiveTimeoutResult);
		//printf("Receive socket timeout was set to %d\n\n", socketReceiveFromServerTimeoutDuration);
		//TEMP


		//Validate setsockopt(.) operation result
		if (SOCKET_ERROR == setClientSocketReceiveTimeoutResult) {
			printf("Error: Failed to alter the socket's 'receive' timeout duration, with error code no. %d.\n", WSAGetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			return TRANSFER_FAILED;
		}
	}

	/*
	if (KEEP_RECEIVE_TIMEOUT != timeoutForNextResponse) {
		FD_ZERO(&set);
		FD_SET(*p_s_communicationSocket, &set);
		timeout.tv_usec = timeoutForNextResponse;
		timeout.tv_sec = 0;

		selectRes = select(
			0,								/ *ignored* /
			&set,							/ * a pointer to a set containing the Server listening socket * /
			NULL,							/ * a pointer to a set of sockets (file descriptors) to be checked for writability - not used * /
			NULL,							/ * a pointer to a set of sockets (file descriptors) to be checked for errors - not used * /
			&timeout						/ * select activation duration - set to input timeout.* /
		);
		printf("Select result    %d\n", selectRes); //'DELETE
	}*/

	//Block the thread (either Worker thread at Server-side, or Speaker thread at Client-side), to receive a message, for a predetermined timeout duration....
	receiveResult = receiveString(&p_receivedMessageBuffer, *p_s_communicationSocket);
	
	//Validate the 'receive' function result codes
	switch (receiveResult) {
	case TRANSFER_FAILED: 
		//This is also the case where Client-side socket stops receiving due to self timeout
		//NOTE: the sentence above in fact shows the true nature of COMMUNICATION_DISCONNECT which was supposed to be the same as TRANSFER_DISCONNECT,
		//		but the latter means that the current side, the executes this function, disconnected Gracefully, thus, COMMUNICATION_DISCONNECT isn't noticed 'CHECK'
		printf("One of the sides either disconnected VIOLENTLY   or   some other failure occured at recv(.)\n");
		return TRANSFER_FAILED; break;
	
	case TRANSFER_DISCONNECTED:
		//Graceful
		return TRANSFER_DISCONNECTED; break;
	
	case TRANSFER_TIMEOUT:
		//Timeout occured during recv(.)
		return TRANSFER_TIMEOUT; break;

	case TRANSFER_SUCCEEDED:
		//Translate (Analyze) the received message into a "message" struct that is divided to the message type & parameters..
		*p_p_receivedMessageInfo = translateReceivedMessageToMessageStruct(p_receivedMessageBuffer);
		//Validate translation result
		if (NULL == *p_p_receivedMessageInfo) {
			//translateReceivedMessageToMessageStruct(.) failed...
			return TRANSFER_FAILED;
		}
		return TRANSFER_SUCCEEDED;

	default: /*ignored*/
		printf("Default: not supposed to reach here.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return TRANSFER_FAILED; break;
	}

}


static int fetchMessageStringLength(const char* p_stringBuffer)
{
	int currentLength = 0;
	const char* p_currentPointerPosition = p_stringBuffer;
	//Assert
	assert(NULL != p_stringBuffer);

	//Since the Buffer containing the message's bytes (and is described as a string of characters) MUST
	// end with a 'LineFeed' character - '\n', AND may contain zero characters '\0', then NO STRING FUNCTIONS
	// may be used!!!   so the length of the message, either for sending or receiving will be calculated manually!!!
	while ('\n' != *(p_stringBuffer + currentLength))
		currentLength++;
	
	//When p_stringBuffer in currentLength position is the Line Feed character, then the end of the message was reached 
	// and currentLength has become the TRUE length of the message in the buffer!
	return currentLength;
}


/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/


communicationResults gracefulDisconnect(SOCKET* p_s_socket)
{
	char* p_blankBuffer = NULL;
	int socketReceiveOperationTimeoutDuration = GRACEFUL_DISCONNECT_WAITING_TIMEOUT, setSocketReceiveTimeoutResultForClosure = 0;
	//Assert
	assert(NULL != p_s_socket);

	//Attempt shutting down the (Client\Server Worker) communication socket for sending operations, 
	// so the (Server Worker\Client) thread socket will receive '0' as a message and will close itself.....
	if (SOCKET_ERROR == shutdown(*p_s_socket, SD_SEND)) {
		printf("Error: Failed to shutdown the socket for sending operation for a 'Graceful Disconnection' procedure, with error code no. %d\nGraceful Disconnect failed!\n", WSAGetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return COMMUNICATION_FAILED;
	}

	//Alter the Client's socket timeout on 'receive' operation to 2 seconds
	setSocketReceiveTimeoutResultForClosure = setsockopt(
		*p_s_socket,										/* socket of either the Client Speaker t.(that communicates with a Server Worker t.) or the Server Worker t.(that communicates with a Client Speaker t.) */
		SOL_SOCKET,											/* the level of the socket in which we set an option's definition - this is the "socket" layer\level */
		SO_RCVTIMEO,										/* the option that will be set in the operation - this is the 'Graceful Disconnect' timeout duration option */
		(void*)&socketReceiveOperationTimeoutDuration,		/* a void type  pointer  to the BUFFER containing bytes of data representing the new value that will be set to the socket's option */
		sizeof(socketReceiveOperationTimeoutDuration));	/* the  size  of the data contained in the BUFFER */

	//Validate setsockopt(.) operation result
	if (SOCKET_ERROR == setSocketReceiveTimeoutResultForClosure) {
		printf("Error: Failed to alter the socket's 'receive' timeout duration, with error code no. %d.\nGraceful Disconnect failed!\n", WSAGetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return COMMUNICATION_FAILED;
	}

	//Block the Client thread to receive the Server Worker's thread shutdown validation
	if (TRANSFER_DISCONNECTED == receiveString(&p_blankBuffer, *p_s_socket)) {
		//Graceful disconnect was noticed by the Server
		printf("Graceful Disconnect succeeded!\n");
		return GRACEFUL_DISCONNECT;
	}
	else {
		printf("Graceful Disconnect failed!\n");
		return SERVER_DISCONNECTED;
	}
}

