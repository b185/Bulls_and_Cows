/* MemoryHandling.c
------------------------------------------------------------------------------------------
	Module Description - This module contains functions meant for freeing dynamic memory 
	allocations & for closing WINAPI Handles.
------------------------------------------------------------------------------------------
*/

// Library includes ----------------------------------------------------------------------
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

// Projects includes ---------------------------------------------------------------------
#include "MemoryHandling.h"


// Constants -----------------------------------------------------------------------------
static const BOOL FAILED_TO_CLOSE_HANDLE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;

// Functions declerations ------------------------------------------------------------------
/// <summary>
///  Description - This function receives a "parameter" struct which is a nested-list
///		and frees all elements in the list.
/// </summary>
/// <param name="parameter* p_firstParameter - A pointer to the head of the nested-list('parameter' struct)"></param>
static void freeTheParameters(parameter* p_firstParameter);




// Functions definitions ------------------------------------------------------------------

//......................................HANDLEs................................
void closeHandleProcedure(HANDLE* p_h_handle)
{
	//Input integrity validation plus handle closer & pointer freeing operation
	if (NULL != p_h_handle) {
		//Attempting to close given handle
		if (FAILED_TO_CLOSE_HANDLE == CloseHandle(*p_h_handle)) {
			printf("Error: Failed close handle with code: %d.\n", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n", __FILE__, __LINE__, __func__);
		}

		//Free the Handle pointer pointing at the Heap mem alloc. 
		free(p_h_handle);
	}
}

void closeThreadsProcedure(HANDLE* p_h_threadsHandlesArray, LPDWORD p_threadIds, int numberOfThreads)
{
	int th = 0;
	//Input integrity validation
	if (0 >= numberOfThreads) { 
		printf("Error: Bad inputs to function: %s\n\n", __func__); return;
	}
	//Freeing Threads IDs DWORD array
	if (NULL != p_threadIds) free(p_threadIds);
	//Closing Handles
	if (NULL != p_h_threadsHandlesArray) {
		for (th; th < numberOfThreads; th++) {
			if(NULL != *(p_h_threadsHandlesArray + th))
				if (FAILED_TO_CLOSE_HANDLE == CloseHandle(*(p_h_threadsHandlesArray + th))) {
					printf("Error: Failed close handle with code: %d.\n", GetLastError());
					printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
				}
		}
		//Free threads Handles arrary
		free(p_h_threadsHandlesArray);
	}
}

//......................................SOCKETSs................................
void closeSocketProcedure(SOCKET* p_s_socket)
{
	//Input integrity validation plus socket closer & pointer freeing operation
	if (NULL != p_s_socket) {
		//Attempting to close given socket
		if (SOCKET_ERROR == closesocket(*p_s_socket)) {
			printf("Error: Failed close socket with code: %d.\n", WSAGetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n", __FILE__, __LINE__, __func__);
		}

		//Free the Socket pointer pointing at the Heap mem alloc.
		free(p_s_socket);
	}
}

void closeListeningSocketProcedure(SOCKET* p_s_socket, SOCKADDR_IN* p_service, 
	fd_set* p_serverListeningSocketSet, struct timeval* p_clientsAcceptSelectTimeout)
{
	//Close the Server's listening (main) socket & free its handle's dynamic memory allocation 
	closeSocketProcedure(p_s_socket);
	//Free the socket's local address struct
	if (NULL != p_service)						free(p_service);
	//Free the file descriptors set that contained the socket for validating the "blocking" status of "accept"
	if (NULL != p_serverListeningSocketSet)		free(p_serverListeningSocketSet);
	//Free the timeout struct for the same "blocking" status validation
	if (NULL != p_clientsAcceptSelectTimeout)   free(p_clientsAcceptSelectTimeout);
}






//......................................message struct................................
void freeTheMessage(message* p_message)
{
	//Freeing the Nested-List factor structs in message
	if (p_message->p_parameters != NULL)  freeTheParameters(p_message->p_parameters);
	//Freeing the message struct
	if (p_message != NULL)  free(p_message);
	//For future use: It is possible to define message** p_p_message = &p_message, then, before free(p_message), place p_p_message=&p_message -> free -> *p_p_message= NULL
}

static void freeTheParameters(parameter* p_firstParameter)
{
	parameter* p_currentParameter = NULL;
	//Assert
	assert(p_firstParameter != NULL);
	//Freeing all the Nested-List of factor-data cells
	while (p_firstParameter != NULL) {
		//Updating the current cell(factor) to be the smallest factor
		p_currentParameter = p_firstParameter;
		//If the Nested-List still has factor structures in it, the smallest pointer is updated to next factor struct(cell)
		if (p_firstParameter->p_nextParameter != NULL)  p_firstParameter = p_firstParameter->p_nextParameter;
		else p_firstParameter = NULL;
		//Freeing the current smallest factor struct(cell)
		free(p_currentParameter);
	}
}


//......................................messageString struct................................

void freeTheString(messageString* p_messageStringStruct)
{
	//Freeing the dynamically allocated memory of the string
	if (NULL != p_messageStringStruct->p_messageBuffer) free(p_messageStringStruct->p_messageBuffer);
	//Freeing the message string struct
	if (NULL != p_messageStringStruct) free(p_messageStringStruct);
}








//.......................................workingThreadPackage struct....................................

void freeTheWorkingThreadPackages(workingThreadPackage** p_p_threadParameters)
{
	workingThreadPackage* p_tempPackage = NULL;
	int p = 0;
	if (NULL != p_p_threadParameters) {
		p_tempPackage = *p_p_threadParameters;
		//Resource 1 - Number of Current Connected Clients to Server
		free(p_tempPackage->p_currentNumOfConnectedClients);
		closeHandleProcedure(p_tempPackage->p_h_connectedClientsNumMutex);
		//Resource 2 - GameSession.txt
		closeHandleProcedure(p_tempPackage->p_h_firstPlayerEvent);
		closeHandleProcedure(p_tempPackage->p_h_secondPlayerEvent);
		//"Exit" & "Error" Events
		closeHandleProcedure(p_tempPackage->p_h_errorEvent);
		closeHandleProcedure(p_tempPackage->p_h_exitEvent);
		//Accept - VALIDATE
		closeSocketProcedure(p_tempPackage->p_s_acceptSocket);
		//Freeing every workingThreadPackage allocated for every Worker thread
		for (p; p < NUM_OF_WORKER_THREADS; p++)
			if(NULL != *(p_p_threadParameters + p))   free(*(p_p_threadParameters + p)); // 'CHECK' FindFirstUnusedThreadSlot
	}
	

	//Free the thread parameters struct (threadPackage)
	free(p_p_threadParameters);
}


void freeThePlayer(workingThreadPackage* p_threadParameters)
{
	if (NULL != p_threadParameters) {
		//Free the memory allcations of ALL numbers & names of both, the Other player & self,
		//	and pointes their pointers to NULL

		//Other current guess
		if (NULL != p_threadParameters->p_otherCurrentGuess) {
			free(p_threadParameters->p_otherCurrentGuess);
			p_threadParameters->p_otherCurrentGuess = NULL;
		}
		//Self current guess
		if (NULL != p_threadParameters->p_selfCurrentGuess) {
			free(p_threadParameters->p_selfCurrentGuess);
			p_threadParameters->p_selfCurrentGuess = NULL;
		}
		//Other initial number
		if (NULL != p_threadParameters->p_otherInitialNumber) {
			free(p_threadParameters->p_otherInitialNumber);
			p_threadParameters->p_otherInitialNumber = NULL;
		}
		//Self initial number
		if (NULL != p_threadParameters->p_selfInitialNumber) {
			free(p_threadParameters->p_selfInitialNumber);
			p_threadParameters->p_selfInitialNumber = NULL;
		}
		//Other name
		if (NULL != p_threadParameters->p_otherPlayerName) {
			free(p_threadParameters->p_otherPlayerName);
			p_threadParameters->p_otherPlayerName = NULL;
		}
		//Self name  - 'CHECK'
		if (NULL != p_threadParameters->p_selfPlayerName) {
			free(p_threadParameters->p_selfPlayerName);
			p_threadParameters->p_selfPlayerName = NULL;
		}
	}
}
