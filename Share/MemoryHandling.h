/* MemoryHandling.h
--------------------------------------------------------------
	Module Description - header module for MemoryHandling.c
--------------------------------------------------------------
*/


#pragma once
#ifndef __MEMORY_HANDLING_H__
#define __MEMORY_HANDLING_H__


// Library includes --------------------------------------------
#include <stdio.h>
#include <string.h>


// Projects includes -------------------------------------------
#include "HardCodedData.h"


//Functions Declarations
/// <summary>
///	 Description - This function receives a pointer to a Handle and attempts to close it and frees the allocated memory. 
///		If it fails to close it, it prints a relevant message.
/// </summary>
/// <param name="HANDLE* p_h_handle - A pointer to a WINAPI Handle to some sychronous object or file"></param>
void closeHandleProcedure(HANDLE* p_h_handle);
/// <summary>
///	 Description - This function receives a pointer to a threads' Handles array, a DWORD array of the threads IDs and a number of threads, and
///		 attempts to close all the opened handles to these threads and frees the allocated memory of the Handles array and the IDs. 
/// </summary>
/// <param name="HANDLE* p_h_threadsHandlesArray - A pointer to a threads Handles array"></param>
/// <param name="LPDWORD p_threadIds - A pointer to DWORD array where every cell represents some thread ID"></param>
/// <param name="int numberOfThreads - An integer representing the number of threads used"></param>
void closeThreadsProcedure(HANDLE* p_h_threadsHandlesArray, LPDWORD p_threadIds, int numberOfThreads);



/// <summary>
///	 Description - This function receives a pointer to a Socket and attempts to close it in terms of connectivity & frees the allocated memory. 
///		If it fails to close it, it prints a relevant message.
/// </summary>
/// <param name="SOCKET* p_s_socket - A pointer to a Winsock Socket"></param>
void closeSocketProcedure(SOCKET* p_s_socket);

/// <summary>
///	 Description - This function receives a pointer to a Socket, its local Socket address, a file descriptor set struct pointer
/// & a 'timeval' struct pointer (that assisted with A-Synch I\O), and attempts to close the Socket in terms of connectivity 
/// & frees the allocated memory of all data structures inputted. 
/// </summary>
/// <param name="SOCKET* p_s_socket - A pointer to a Winsock Socket"></param>
/// <param name="SOCKADDR_IN* p_service - A pointer to the local socket address that was previously bound to the socket inputted"></param>
/// <param name="fd_set* p_serverListeningSocketSet - A pointer to a mem allocated 'file descriptor set' data struct"></param>
/// <param name="struct timeval* p_clientsAcceptSelectTimeout - pointer to a mem allocated 'timeval' data struct"></param>
void closeListeningSocketProcedure(SOCKET* p_s_socket, SOCKADDR_IN* p_service,
	fd_set* p_serverListeningSocketSet, struct timeval* p_clientsAcceptSelectTimeout);



/// <summary>
/// Description - This function receives a "message" struct and frees the "parameter" struct nested list within it, and then it frees the "message" struct itself.
/// </summary>
/// <param name="message* p_message - A pointer to a 'message' datatype (struct) that was used to store a message's parameters & a message's type"></param>
void freeTheMessage(message* p_message);
/// <summary>
/// Description - This function receives a "messageString" struct and frees the string it points at, and then it frees the "messageString" struct itself.
/// </summary>
/// <param name="messageString* p_messageStringStruct - A pointer to a 'messageString' struct"></param>
void freeTheString(messageString* p_messageStringStruct);


/// <summary>
/// Description - This function receives a "workingThreadPackage" array of pointers to structs and frees all of the data it points at (almost)
///  e.g. "1st Player"\"2nd Player" Events Handles, "ERROR"\"EXIT" Events Handles, "accepted" socket attained form the accept(.) function
///  and connected to a Client. and then it frees the "threadPackage" structs array pointer itself.
/// </summary>
/// <param name="workingThreadPackage** p_p_threadParameters - A pointer to pointers of 'workingThreadPackage' structs that was used to hold all the parameters for the threads"></param>
void freeTheWorkingThreadPackages(workingThreadPackage** p_p_threadParameters);

/// <summary>
///  Description - This function receives a "workingThreadPackage" struct pointer frees all of the data in it that points at a couple of players
/// in a game e.g. "self player" Initial number and Guesses(one guess at a time. guesses were allocated & erased at every game phase) number,
/// & "other player" Name, Initial number and Guesses number.
/// </summary>
/// <param name="workingThreadPackage* p_threadParameters - pointer to a 'workingThreadPackage' struct that was used for a single Worker thread"></param>
void freeThePlayer(workingThreadPackage* p_threadParameters);

#endif //__MEMORY_HANDLING_H__