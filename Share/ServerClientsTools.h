/* ServerClientsTools.h
-----------------------------------------------------------------
	Module Description - header module for ServerClientsTools.c
-----------------------------------------------------------------
*/


#pragma once
#ifndef __SERVER_CLIENTS_TOOLS_H__
#define __SERVER_CLIENTS_TOOLS_H__


// Library includes -------------------------------------------------------
#include <Windows.h>



// Projects includes ------------------------------------------------------
#include "HardCodedData.h"
#include "MemoryHandling.h"
#include "MessagesTransferringTools.h"

//Functions Declarations

/// <summary>
/// Description - This function creates a new thread and attaches a Handle to it. It channels the thread a unique "workingThreadPackage" struct (if exists)
///		and a unique ID from the threadIds array (or single id variable).The threads are set to have a default stack size(code - 0)
///		and a default security descriptor(Sec.Att. - null).
/// </summary>
/// <param name="LPTHREAD_START_ROUTINE p_startRoutine - A pointer to the thread's routine address"></param>
/// <param name="LPVOID p_threadParameters - A void pointer to the unqiue 'threadPackage' data struct(Worker\Speaker threads) or NULL in the case of "Exit" procedure thread"></param>
/// <param name="LPDWORD p_threadId - A pointer to ID's address"></param>
/// <returns>A Handle to the thread if successful, or NULL if failed (validated outside of this function)</returns>
HANDLE createThreadSimple(LPTHREAD_START_ROUTINE p_startRoutine, LPVOID p_threadParameters, LPDWORD p_threadId);

/// <summary>
/// Description - Create an Event (can be named) with a given Initial state (signaled or non-signaled) and  given Reset protocol
/// (manual or auto), and return a Handle to that Event object.
/// </summary>
/// <param name="BOOL eventManualAutoReset - Manual\Auto reset bit"></param>
/// <param name="BOOL eventInitialState - Initialized in Signaled\Non-Signaled bit"></param>
/// <param name="LPCSTR p_eventName - pointer to a string representing the Event Object name if given"></param>
/// <returns>A pointer to the Event's Handle if succeded, or NULL otherwise</returns>
HANDLE* allocateMemoryForHandleAndCreateEvent(BOOL eventManualAutoReset, BOOL eventInitialState, LPCSTR p_eventName);



/// <summary>
///  Description - This function receives a pointer to a Handles array and activates WaitForMultipleObjects. It validates
///		whether all threads terminated on time(According to the time cap), or if anything else failed.
/// </summary>
/// <param name="HANDLE* p_h_threadHandles - A pointer the threads Handles array."></param>
/// <param name="int numberOfThreads - The number of threads"></param>
/// <param name="int timeout - timeout value"></param>
/// <returns>WAIT_OBJECT_0 (all threads finished)  ;  WAIT_TIMEOUT(some or all didn't finish)   ;   WAIT_ABANDONED_0   ;   WAIT_FAILED (something failed)</returns>
int validateThreadsWaitCode(HANDLE* p_h_threadHandles, int numberOfThreads, int timeout);
/// <summary>
///  Description - This function receives a pointer to a Server Worker thread and validates how the thread terminated.
/// </summary>
/// <param name="HANDLE* p_h_threadHandle - A pointer the thread Handle."></param>
/// <returns>A set of value described by the 'communicationResults' enums described in HardCodedData.h </returns>
BOOL validateThreadExitCode(HANDLE* p_h_threadHandle);

/// <summary>
/// Description - The function access the "Current Connected Clients Number" resource with its Mutex and alters it (or not) depending on the inputted
/// incremented\decremented value: 0 - don't change and just fetch value,  1 - increment(when Client is connected), -1 - decrement (when Client disconnected).
/// The function attempts to access the resource through the resource according to a predetermined timeout which value was chosen mainly according to the SHORT
/// recv(.) timeout duration.....
/// </summary>
/// <param name="USHORT* p_currentNumOfConnectedClients - pointer to resource"></param>
/// <param name="HANDLE* p_h_connectedClientsNumMutex - pointer to the resource's Mutex Handle"></param>
/// <param name="int increDecreVal - Incrementation\Decrementation\Fetch value"></param>
/// <param name="long timeout - timeout duration when attempting to own the mutex"></param>
/// <returns>The number of currently connected clients to the Server or -1 if FATAL ERROR occured</returns>
int incrementDecrementCheckNumberOfConnectedClients(USHORT* p_currentNumOfConnectedClients, HANDLE* p_h_connectedClientsNumMutex, int increDecreVal, long timeout);



/// <summary>
/// Description - A wrapper that can send any chosen message directed to the Server from a Client through a given socket, after assembling the message
/// into a buffer comprised of the message types and given parameters...
/// </summary>
/// <param name="SOCKET* p_s_clientCommunicationSocket - communication socket"></param>
/// <param name="int messageTypeSerialNumber - message type value as determined in HardCodedData.h"></param>
/// <param name="char* p_paramOne - pointer to buffer of a single paramter if exists (Clients' messages may have up to a single parameter only)"></param>
/// <returns>'communicationResults' enum value which may be COMMUNICATION_SUCCEEDED(if sending succeeded) ; COMMUNICATION_FAILED(if fatal error occured e.g. mem alloc.) ; SERVER_DISCONNECT(if send(.) failed which was probably cause by disconnection from server)</returns>
communicationResults sendMessageClientSide(SOCKET* p_s_clientCommunicationSocket, int messageTypeSerialNumber, char* p_paramOne);

/// <summary>
/// Description - A wrapper that can send any chosen message directed to a Client from the Server  through a given socket, after assembling the message
/// into a buffer comprised of the message types and given parameters...
/// </summary>
/// <param name="SOCKET* p_s_clientCommunicationSocket - pointer to acommunication socket"></param>
/// <param name="int messageTypeSerialNumber - message type value as determined in HardCodedData.h"></param>
/// <param name="char* p_paramOne, *p_paramTwo, *p_paramThree, *p_paramFour - pointers to the buffers of paramters if exists (Servers' messages may have up to four parameters)"></param>
/// <returns>'communicationResults' enum value which may be COMMUNICATION_SUCCEEDED(if sending succeeded) ; COMMUNICATION_FAILED(if fatal error occured e.g. mem alloc.) ; SERVER_DISCONNECT(if send(.) failed which was probably cause by disconnection from server)</returns>
communicationResults sendMessageServerSide(SOCKET* p_s_serverCommunicationSocket, int messageTypeSerialNumber,
	char* p_paramOne, char* p_paramTwo, char* p_paramThree, char* p_paramFour);


/// <summary>
///  Description - A wrapper that can receive any chosen message in any direction (Client<-->Server)  through a given socket, 
/// and seperating the given data(buffer) into a 'message' stuct containing the fields of the message type (number) and nested list of parameters buffers.
/// Also, it receives a recv(.) timeout value, and sets the given socket receive operation timeout duration using setsockopt(.) Winsock API func.
/// </summary>
/// <param name="SOCKET* p_s_clientCommunicatonSocket - pointer to a communication Socket"></param>
/// <param name="message** p_p_receivedMessageInfo - pointer address of 'message' struct that will be allocated memory to, if operation succeeded"></param>
/// <param name="int responseReceiveTimeoutValue - new(or don't change = -1) timeout duration value of recv(.)"></param>
/// <returns>'communicationResults' enum value which may be COMMUNICATION_SUCCEEDED(if receive succeeded) ; COMMUNICATION_FAILED(if fatal error occured e.g. mem alloc.) ; SERVER_DISCONNECT(if recv(.) failed which was probably caused by disconnection from server) ; COMMUNICATION_TIMEOUT(if recv(.) timedout)</returns>
transferResults receiveMessage(SOCKET* p_s_clientCommunicatonSocket, message** p_p_receivedMessageInfo, int responseReceiveTimeoutValue);


/// <summary>
/// Description - This function performs the 'Graceful Disconnect' procedure by using shutdown and using recv(.) to receive byte-read value of 0.
/// If the procedure doesn't detect the desired 'receive' operation outcome within a timeout of 2 seconds (predetermined according to SHORT Client\Server response),
/// it leaves without a fatal error code (COMMUNICATION_FAILED) but rather an abrupt disconnection code (COMMUNICATION_DISCONNECT)
/// </summary>
/// <param name="SOCKET* p_s_socket - pointer to a communication Socket"></param>
/// <returns>/// <returns>'communicationResults' enum value which may be GRACEFUL_DISCONNECT(if succeeded) ; COMMUNICATION_FAILED(if fatal error occured e.g. mem alloc.) ; SERVER_DISCONNECT(if recv(.) failed which was probably caused by disconnection from server)</returns></returns>
communicationResults gracefulDisconnect(SOCKET* p_s_socket);

#endif //__SERVER_CLIENTS_TOOLS_H__
