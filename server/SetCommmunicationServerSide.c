/* setCommmunicationServerSide.c
---------------------------------------------------------------------------------
	Module Description - This module contains functions meant for setting up the 
		Server side listening socket & for accepting new clients with new sockets
		and create Worker threads to communicate with Clients Speaker threads.
		Also it initiates an 'Exit' thread routine to receive an STDin input of 
		'exit' when it arrives.
---------------------------------------------------------------------------------
*/


//'CHECK'
//'FUNC'


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
#include "HardCodedData.h"
#include "MemoryHandling.h"
#include "ServerClientsTools.h"
#include "ServerSideWorkerThreadRoutine.h"




// Constants
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;

	//0o0o0o Server process closure status constants
//static const int KEEP_GOING = 0;
//static const BOOL STATUS_SERVER_ERROR = -1;
//static const BOOL STATUS_SERVER_EXIT = -2;

//static const short NUM_OF_WORKER_THREADS = 3;
//static const short DEFAULT_THREAD_STACK_SIZE = 0;
	
	//0o0o0o timeval struct time duration constants
static const long SELECT_TIMEOUT_SEC = 14;
static const long SELECT_TIMEOUT_MILLI_SEC = 500;


static const BOOL INETPTONS_SUCCESS = 1;
static const long CUR_CON_CLI_MUTEX_OWNERSHIP_TIMEOUT = 2000; // 2 Seconds - Current Connected Clients count timeout


	//0o0o0o Events initialization parameters values
static const BOOL MANUAL_RESET = TRUE;
static const BOOL AUTO_RESET = FALSE;
static const BOOL INITIALLY_SIGNALED = TRUE;
static const BOOL INITIALLY_NON_SIGNALED = FALSE;

static const int SAMPLE = 0;
static const int SINGLE_OBJECT = 1;

static const int EXIT_CASE_THREADS_TIMEOUT = 15000; // 0.5 Seconds CHANGED to 15s
static const int ERROR_CASE_THREADS_TIMEOUT = 15000; // 15 Seconds

static const BOOL  GET_EXIT_CODE_FAILURE = 0;

// Global variables ------------------------------------------------------------
//First, it is convenient to count the number of current connected clients, mainly, for the purpose of rejecting a third player.
// For that end, a "USHORT" datatype variable will be defined, and also a Mutex Synchronous object.
// The reason the Mutex is needed is because all Clients-handling threads in the Server side will utilize this Mutex to
// either read or update its value e.g. when a player(Client) leaves the game room, and its thread in the Server terminates,
// we would like to notify the main thread of the Server process it can initiate an additional game with two players.
USHORT* g_p_currentNumOfConnectedClients = NULL;
HANDLE* g_p_h_connectedClientsNumMutex = NULL;

//Second, the "Exit", "Error" Events pointers will be set to be global pointers as well to ease the visuality of the code
// events set to be manual-reset & initialy non-signaled
//HANDLE* p_h_exitErrorEvent = NULL;
HANDLE* g_p_h_exitEvent = NULL;
HANDLE* g_p_h_errorEvent = NULL;


//Third, all the GameSession.txt resource synchronous objects' pointers will also be defined as global pointers for ease
HANDLE* g_p_h_firstPlayerEvent = NULL;
HANDLE* g_p_h_secondPlayerEvent = NULL;






// Functions declerations ------------------------------------------------------

/// <summary>
/// Description - 'Error' indicator thread routine. After activation will 'sit' in wait until 'exit' is inserted to STDin, then it will signal the 
/// 'EXIT' event and will begin termination procedure
/// </summary>
/// <param name="lpParam - ignored"></param>
/// <returns>True if 'exit' was inserted and event was set to signaled successfuly, False otherwise</returns>
static BOOL WINAPI exitErrorThreadRoutine(LPVOID lpParam);


/// <summary>
/// Description - This function will allocate memory for a SOCKET struct, use socket(.) to create a socket, create a SOCKADDR_IN struct with a local
/// address associated with the input port number, bind(.) the socket to that address,  and will initiate listening using listen(.)
/// </summary>
/// <param name="SOCKET** p_p_s_mainServerSocket - pointer address to a SOCKET struct"></param>
/// <param name="SOCKADDR_IN** p_p_service - pointer address to a SOCKADDR_IN struct"></param>
/// <param name="unsigned short serverPortNumber - port number ranging from 0 to 65536"></param>
/// <returns>True if operation succeeded, False if otherwise</returns>
static BOOL socketCreationAndLocalAddressBindingAndListening(SOCKET** p_p_s_mainServerSocket, SOCKADDR_IN** p_p_service, unsigned short serverPortNumber);
/// <summary>
/// Description - to overcome the "Blocking" of accept(.) function, this function will create a file descriptor set struct and timeval struct.
/// The set will containg the listening socket and timval will contain the blocking timeout duration of ever accept operation.. (will be set to 14 sec - there was
/// no indication of a certain value in the instructions)
/// </summary>
/// <param name="SOCKET* p_s_serverSocket - pointer to the listening socket"></param>
/// <param name="fd_set** p_p_serverListeningSocketSet - pointer address to the fd_set struct"></param>
/// <param name="struct timeval** p_p_clientsAcceptSelectTimeout - pointer address to the timeval struct whose values will be modified"></param>
/// <returns>True if operation succeeded, False if otherwise</returns>
static BOOL setServerListeningSocketInSocketsSetAndAdjustAcceptTimeout(SOCKET* p_s_serverSocket, fd_set** p_p_serverListeningSocketSet, struct timeval** p_p_clientsAcceptSelectTimeout);
/// <summary>
/// Description - This function will allocate dynamic memory for the Server Worker Handles (currently 3, cause instruction said 3 are the upper number of clients attempting to connect)
/// and another Handle to the 'exit' routine. Also Thread IDs array will be allocated
/// </summary>
/// <param name="HANDLE** p_p_h_clientsThreadsHandles - pointer address to the Worker thread Handle arrary"></param>
/// <param name="LPDWORD* p_p_threadIds - pointer address to the thread ID array"></param>
/// <param name="HANDLE** p_p_h_exitThread - pointer address to the exit thread handle"></param>
/// <returns>True if operation succeeded, False if otherwise</returns>
static BOOL createThreadsHandlesAndTheirIds(HANDLE** p_p_h_clientsThreadsHandles, LPDWORD* p_p_threadIds, HANDLE** p_p_h_exitThread);
/// <summary>
/// Description - This function will simply create the synchronous objects and gain a handle to them needed in the server,
///  and attach them to the Worker thread input structs array workingThreadPackage**:
/// Events for 'exit', fatal errors, 1st Player, 2nd Player
/// Mutex for the number of "Currently Connected Clients"
/// The number of "Currently Connected Clients"
/// It will also call the createThreadPackageAndInsertSynchronousObjectsPointerToThem(.) to bind all pointer to a workingThreadPackage for every potential Worker thread
/// </summary>
/// <returns>pointer to the created and updated workingThreadPackage array</returns>
static workingThreadPackage** createSynchronousObjectsAndInsertTheirPointersToThreadPackages();
/// <summary>
/// Description - this function will create for every Worker thread indevidually his own input package struct, and bind the Synch objects and resource pointers to it
/// </summary>
/// <returns>pointer to a created and updated workingThreadPackage of some thread</returns>
static workingThreadPackage* createThreadPackageAndInsertSynchronousObjectsPointerToThem();
/// <summary>
/// Description - Mutex synch object creation, not initially own and un-named
/// </summary>
/// <returns>pointer to the allocated Handle attached to the mutex</returns>
static HANDLE* allocateMemoryForHandleAndcreateMutex();


/// <summary>
/// Description - This function will find the smallest index of a currently idle Worker thread
/// </summary>
/// <param name="HANDLE* p_h_clientsThreadsHandles - pointer to an array of Handles to Worker threads"></param>
/// <returns>index of one them if found, NUM_OF_WORKER_THREADS if there is no idle thread at the time of the calling </returns>
static int findFirstUnusedThreadSlot(HANDLE* p_h_clientsThreadsHandles);
/// <summary>
/// Description - in case a connection was accepted and new socket has been created, this function will create a thread with input package that contains
/// the address of this new socket, and begin communication with the Client
/// </summary>
/// <param name="SOCKET* p_s_acceptSocket - new accept socket"></param>
/// <param name="HANDLE* p_h_clientsThreadsHandles - pointer to threads handle array"></param>
/// <param name="LPDWORD p_threadIds - pointer to the Worker threads ID array"></param>
/// <param name="workingThreadPackage** p_p_threadPackages - pointer to the array of all available Worker threads input packages"></param>
/// <returns>operation result: KEEP GOING if successful, something else if failed</returns>
static int findIdleWorkerThreadForTheNewConnectedClientAndInitiate(SOCKET* p_s_acceptSocket/*Stack mem.*/, HANDLE* p_h_clientsThreadsHandles, LPDWORD p_threadIds, workingThreadPackage** p_p_threadPackages);

/// <summary>
/// Description - This function validates the status of the 'Exit' 'Error' events and operates accordigly - KEEP GOING is continue listening loop
/// </summary>
/// <param name="int exitFlag - current Connection loop continuation status"></param>
/// <returns>updated Connection loop continuation status</returns>
static int validateErrorExitEventsStatus(int exitFlag);

/// <summary>
/// Description - This function terminates a thread if it is still active by the time the function is called
/// </summary>
/// <param name="HANDLE* p_h_threadHandle - pointer to the thread handle"></param>
static void threadTermination(HANDLE* p_h_threadHandle);

/// <summary>
/// Description - this function performs a termination procedure for all threads (besides the Server's main thread) under certain timeout conditions
/// </summary>
/// <param name="HANDLE* p_h_clientsThreadsHandles - pointer to Worker threads Handles array"></param>
/// <param name="HANDLE* p_h_exitThread - pointer to  exit  thread handle"></param>
/// <param name="int timeout - timeout duration until  termination"></param>
/// <returns>Suceess codes</returns>
static int threadsStatusValidationAndHandlingBeforeExiting(HANDLE* p_h_clientsThreadsHandles, HANDLE* p_h_exitThread, int timeout);

/// <summary>
/// /// <summary>
/// Descirption - This function frees all memory, await\terminates all threads and validate their exit codes, and closes the listening socket operation...
/// </summary>
/// </summary>
/// <param name="int exitFlag - process status indicator"></param>
/// <param name="SOCKET* p_s_mainServerSocket - pointer to socket "></param>
/// <param name="SOCKADDR_IN* p_service - pointer to socket address struct"></param>
/// <param name="fd_set* p_serverListeningSocketSet - pointer to fd set"></param>
/// <param name="struct timeval* p_clientsAcceptSelectTimeout - pointer to  timeval struct"></param>
/// <param name="HANDLE* p_h_clientsThreadsHandles - pointer to Worker threads Handles array"></param>
/// <param name="LPDWORD p_threadIds - pointer to the Worker threads ID array"></param>
/// <param name="HANDLE* p_h_exitThread - pointer to  exit  thread handle"></param>
/// <param name="p_p_threadPackages - pointer the Worker threads input package array"></param>
/// <returns>True if Server operation succeeded. False if otherwise</returns>
static BOOL cleanupAndExit(int exitFlag,
	SOCKET* p_s_mainServerSocket,
	SOCKADDR_IN* p_service,
	fd_set* p_serverListeningSocketSet,
	struct timeval* p_clientsAcceptSelectTimeout,
	HANDLE* p_h_clientsThreadsHandles,
	LPDWORD p_threadIds,
	HANDLE* p_h_exitThread,
	workingThreadPackage** p_p_threadPackages);

// Functions definitions -------------------------------------------------------

BOOL setCommmunicationServerSide(unsigned short serverPortNumber)
{
	//Winsock connectivity variables & pointers
	WSADATA wsaData;
	SOCKET* p_s_mainServerSocket = NULL, *p_s_acceptSocket = NULL;
	SOCKADDR_IN* p_service = NULL;
	fd_set* p_serverListeningSocketSet = NULL;
	struct timeval *p_clientsAcceptSelectTimeout= NULL;
	//Threads pointers
	HANDLE* p_h_clientsThreadsHandles = NULL, *p_h_exitThread = NULL;
	LPDWORD p_threadIds = NULL;
	DWORD exitThreadId = 0;
	//Working Thread package - Synchronous objects handles pointers & "accept" socket pointer
	workingThreadPackage** p_p_threadPackages = NULL;
	//Others
	int selectResult = 0, exitFlag = 0;
	
	// Input integrity validation
	//maybe skip

	// Initialize Winsock.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("Error: Failed to initalize Winsock API using WSAStartup( ) with error code no. %ld.\n Exiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}
	/* The WinSock DLL is acceptable. Proceed. */





	// Create a socket & bind a socket address to it, comprised of a valid IP address (e.g. 127.0.0.1) and the input port number 
	//		& initiate listening to incoming Clients connections. 
	// Note: This function updates BOTH a SOCKET & a SOCKADDR_IN pointers and pointes them to an allocated & updated SOCKET & SOCKADDR_IN 
	//		 structs respectively. The SOCKET created is the Server's listening socket.
	if (STATUS_CODE_FAILURE == socketCreationAndLocalAddressBindingAndListening(&p_s_mainServerSocket ,&p_service, serverPortNumber)) {
		if (SOCKET_ERROR == WSACleanup())
			printf("Error: Failed to close Winsocket, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}


	// Create a "sockets set" (file descriptors set struct) to contain the Server's listening socket ONLY so it is possible
	//	to sample the socket for awaiting Clients' connections to be approved without being BLOCKED by "accept"(Winsock2) function,
	//	mainly, when there are no Clients currently requesting to connect to the server.
	if (STATUS_CODE_FAILURE == setServerListeningSocketInSocketsSetAndAdjustAcceptTimeout(p_s_mainServerSocket, &p_serverListeningSocketSet, &p_clientsAcceptSelectTimeout)) {
		//Free resources: Socket & socket local address
		closeListeningSocketProcedure(p_s_mainServerSocket, p_service, NULL, NULL);
		if (SOCKET_ERROR == WSACleanup())
			printf("Error: Failed to close Winsocket, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE; //All other resources were freed inside the function
	}




	// Initialize all thread handles: The threads that are purposed to communicate with connected clients (a.k.a Worker Threads) & the
	//	EXIT\ERROR thread that is purposed to notify ALL threads of the Server (including the main thread) that either an "Exit" was inserted
	//	by the Server user or a fatal error has occured in one of the threads when, for example, a thread attempted to allocate
	//	memory for a data struct  and failed OR when a thread attempted to own a resource Mutex and fataly failed.
	//NOTE: The Worker Threads will be set to NULL to mark that they have not been initialized & Their IDs arrary will be allocated
	//		and its pointer will be updated
	if (STATUS_CODE_FAILURE == createThreadsHandlesAndTheirIds(&p_h_clientsThreadsHandles, &p_threadIds, &p_h_exitThread)) {
		
		closeListeningSocketProcedure(p_s_mainServerSocket, p_service, p_serverListeningSocketSet, p_clientsAcceptSelectTimeout);
		if (SOCKET_ERROR == WSACleanup())
			printf("Error: Failed to close Winsocket, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}






	//The following operation will create one of the needed resources and all Synchronous objects, by allocating Heap memory for their handles &
	//	creating the objects with WINapi. Also, this function will place the relevant pointers into the Working threads inputs struct.
	//NOTE: some of these sync objects' pointers are defined as global, because the main Server thread & the Exit\Error thread will use 
	//		some of these objects
	if (NULL == (p_p_threadPackages = createSynchronousObjectsAndInsertTheirPointersToThreadPackages())) {
		
		closeListeningSocketProcedure(p_s_mainServerSocket, p_service, p_serverListeningSocketSet, p_clientsAcceptSelectTimeout);
		free(p_h_clientsThreadsHandles);
		free(p_threadIds);
		free(p_h_exitThread);
		if (SOCKET_ERROR == WSACleanup())
			printf("Error: Failed to close Winsocket, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}



	//Initiate "Exit" thread - when "Exit" in inserted to STDin then the program must finish and quit
	// For that end, and Event will be signaled from that thread.
	if (INVALID_HANDLE_VALUE == (*p_h_exitThread = createThreadSimple(
		(LPTHREAD_START_ROUTINE)exitErrorThreadRoutine,		/* "Exit" thread routine */
		NULL,												/* no - parameters needed */
		&exitThreadId))) {									/* thread ID number address */
		
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		closeListeningSocketProcedure(p_s_mainServerSocket, p_service, p_serverListeningSocketSet, p_clientsAcceptSelectTimeout);
		free(p_h_clientsThreadsHandles);
		free(p_threadIds);
		free(p_h_exitThread); 
		freeTheWorkingThreadPackages(p_p_threadPackages); 
		if (SOCKET_ERROR == WSACleanup())
			printf("Error: Failed to close Winsocket, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}


	printf("Waiting for a client to connect... \n");
	
	while(KEEP_GOING == exitFlag) //DIFFERENT CONDITION
	{
		
		//Query "accept" to understand if it would  block, meaning, check if there are no Clients attempting to connect the Server
		selectResult = select(
			0,								/*ignored*/
			p_serverListeningSocketSet,		/* a pointer to a set containing the Server listening socket */
			NULL,							/* a pointer to a set of sockets (file descriptors) to be checked for writability - not used */
			NULL,							/* a pointer to a set of sockets (file descriptors) to be checked for errors - not used */
			p_clientsAcceptSelectTimeout	/* select activation duration - set to 4.5 seconds. can be altered by altering the constants above^ */
		);
		//RESET the file descriptors READERS set, that contains the main Server thread socket (Listening socket)
		FD_SET(*p_s_mainServerSocket, p_serverListeningSocketSet);

		//If there is a Client currently pending to connect to the Server then the Server will allocate that connection to a new socket
		// and to a designated thread. If timeout reached then it means that there are no Clients currently attempting to connect the Server.
		switch (selectResult) {

		case CLIENT_AWAITS_CONNECTION: //"accept" wouldn't block if used
			//SOCKET struct dynamic memory allocation
			if (NULL == (p_s_acceptSocket = (SOCKET*)calloc(sizeof(SOCKET), SINGLE_OBJECT))) {
				printf("Error: Failed to allocate memory for an accepted SOCKET struct.\n");
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				exitFlag = STATUS_SERVER_ERROR;
				break;
			}
			
			*p_s_acceptSocket = accept(*p_s_mainServerSocket, NULL, NULL);
			if (INVALID_SOCKET == *p_s_acceptSocket) { //It was notified that a 4th (or more) player will NOT attempt to connect when there are already 3 players (Clients) connected
				printf("Error: Failed to accept connection with a new client, with error no. %ld.\n", WSAGetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				//notify threads, terminate, and exit
				if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)) {
					printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
					printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
					free(p_s_acceptSocket);
					//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
				}
				exitFlag = STATUS_SERVER_ERROR;
				break;
			}

			// A Client (player) has been Accepted into a connection with the Server, and the communication between then was trasffered to a new socket
			printf("Client Connected.\n");
			exitFlag = findIdleWorkerThreadForTheNewConnectedClientAndInitiate(p_s_acceptSocket, p_h_clientsThreadsHandles, p_threadIds, p_p_threadPackages);
			//....Proceed to validate "Exit"\"Error" Events' status....
			
			break;

		case NO_CLIENT_PENDING_CONNECTION: //"accept" would block until a Client trys to connect if used
			//....Proceed to validate "Exit"\"Error" Event's status....
			break;

		case SOCKET_ERROR:
			printf("Error: Failed to sample 'accept' function status before activating, with error code no. %ld\n", WSAGetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
			//Signal "Exit"\"Error" Event 
			if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)) {
				printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				//Continue to exiting regardless of failure setting "Error" event
			}

			exitFlag = STATUS_SERVER_ERROR;
			break;

		default: /*ignore*/ break;
		
		} // switch (selectResult)

		//TEMPPPPPPPPP
		//closeListeningSocketProcedure(p_s_mainServerSocket, p_service, p_serverListeningSocketSet, p_clientsAcceptSelectTimeout);


		/* --------------------------------------------------------------------------------- */
		/*....................Check whether the "Exit"\"Error" Event was signaled		     */
		/* --------------------------------------------------------------------------------- */
		exitFlag = validateErrorExitEventsStatus(exitFlag);
		


	} // while(TRUE)


	//Either when "Error" event is signaled or "Exit" event is signaled, the function will
	//	empty all resources, validate threads wait & exit codes or terminate the threads and return a Success\Failure code...
	return cleanupAndExit(exitFlag,
		p_s_mainServerSocket, 
		p_service, 
		p_serverListeningSocketSet, 
		p_clientsAcceptSelectTimeout,
		p_h_clientsThreadsHandles, //TERMINATETHREAD MAY BE USED ON AN UNINITIALIZED THREAD HANDLE
		p_threadIds,
		p_h_exitThread,
		p_p_threadPackages);

}








//......................................Static functions..........................................

//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0    STDin "exit" inputted validation thread routing 
static BOOL WINAPI exitErrorThreadRoutine(LPVOID lpParam)
{
	char inputFromServerUser[] = "null"; //"null" was picked arbitrary in order to initialize the variable kept in process Stack
	//......Parameters are not to be used - Expecting NULL parameters



	//Initiate STDin Read. The operation will Block the thread until a string will be inserted by
	//	The Server application's(process) user
	scanf_s("%s", &inputFromServerUser, EXIT_GUESS_LEN); //2 things: 1) Might need to insert to a loop in case of User's typos 2) scanf_s retval 'CHECK'
	//printf("\n%s\n", inputFromServerUser);

	//Validate "exit" was inserted
	if (STRINGS_ARE_EQUAL(inputFromServerUser, "exit", EXIT_GUESS_LEN)) {
		//Validate "Error" Event status
		switch (WaitForSingleObject(*g_p_h_errorEvent, SAMPLE)) {
		case WAIT_TIMEOUT:
			//No error (e.g. dynamic memory allocations, handles creation, sockets creation) has occured yet, so the Server process still functions properly...
			//so the main server thread will signal the "Exit" Event to all threads...    0.0.0.0.0 SET EXIT EVENT 0.0.0.0.0
			if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_exitEvent)) {
				printf("Error: Failed to set 'EXIT' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				return STATUS_CODE_FAILURE; //!!!!!!!!!!!!change accoring to exit codes function
			}
			//Return with a CORRECT exit code!!!!!
			return STATUS_CODE_SUCCESS;
			break;
		case WAIT_OBJECT_0:
			//Some error\s has\have occured during the runtime of the Server process, so all threads were already notified to release all resources and exit..
			//"Exit" event will not be signaled so it is possible to notify the Server process user, the operation failed at some part...
			return STATUS_CODE_FAILURE; break; //!!!!!!!!!!!!change accoring to exit codes function
		default:
			printf("Error: Failed to sample Server-side 'ERROR' Event's status using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
			//Some error has occured during checking the "Error" Event, so it is possible to leave with an error code
			return STATUS_CODE_FAILURE; break; //!!!!!!!!!!!!change accoring to exit codes function
		}
		//All Server-side threads were notified to FINISH....! 'CHECK EXPLAIN'
		//return STATUS_CODE_SUCCESS; //!!!!!!!!!!!!change accoring to exit codes function
	}
	//STDin read failed either because string mismatch or because sscan_f failure.....
	if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)) {
		printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
	}
	return STATUS_CODE_FAILURE;
	
}
//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o 





//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0    Main Server preparation function 
static BOOL socketCreationAndLocalAddressBindingAndListening(SOCKET** p_p_s_mainServerSocket,SOCKADDR_IN** p_p_service, unsigned short serverPortNumber)
{
	SOCKET* p_s_mainServerSocket = NULL;
	SOCKADDR_IN* p_service = NULL;
	int bindResult = 0, listenResult = 0;
	//Assert
	assert(NULL != p_p_s_mainServerSocket);
	assert(NULL != p_p_service);

	//SOCKET struct dynamic memory allocation
	if (NULL == (p_s_mainServerSocket = (SOCKET*)calloc(sizeof(SOCKET), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a SOCKET struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}

	//Create a socket
	if (INVALID_SOCKET == (*p_s_mainServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
		printf("Error: Failed to set a socket using socket( ), with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		//Free the data of the Server's main(listening) socket & set its pointer's pointer to NULL
		free(p_s_mainServerSocket);
		return STATUS_CODE_FAILURE;
	}

	//SOCKADDR_IN struct dynamic memory allocation
	if (NULL == (p_service = (SOCKADDR_IN*)calloc(sizeof(SOCKADDR_IN), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a SOCKADDR_IN struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		//Close & free the data of the Server's main(listening) socket & set its pointer's pointer to NULL
		closeSocketProcedure(p_s_mainServerSocket); //also frees dynamic memory
		return STATUS_CODE_FAILURE;
	}

	/*
		For a server to accept client connections, it must be BOUND to a network address within the system.
		Client applications use the IP address and port to connect to the host network.
		The sockaddr structure holds information regarding the address family, IP address, and port number.
		sockaddr_in is a subset of sockaddr and is used for IP version 4 applications.
   */


	//Update SOCKADDR_IN struct with the relevant IPv4 TCP connection information 
	/*
		The three lines following the declaration of sockaddr_in service are used to set up the sockaddr structure:
	*/
	//AF_INET is the Internet address family (IPv4 addresses family).
	p_service->sin_family = AF_INET;
	//Translate the Server local IP address string to a binary representation ("127.0.0.1" is the local IP address to which the socket may be bound to.)
	if (INETPTONS_SUCCESS != InetPton(AF_INET, SERVER_ADDRESS_STR, &p_service->sin_addr.s_addr)) {
		printf("Error: Failed to translate the input Destination Server IP address to a binary value, with error code no. %d.\n", WSAGetLastError());
		printf("code no. 10022 will most likely indicate an invalid string datatype(char* instead of wchar_t* was entered OR some other mistake\n\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		//Close & free the data of the Server's main(listening) socket & set its pointer's pointer to NULL
		closeSocketProcedure(p_s_mainServerSocket); //also frees dynamic memory
		free(p_service);
		return STATUS_CODE_FAILURE;
	}
	//Input port number is the port number to which the socket will be bound.
	p_service->sin_port = htons(serverPortNumber); //The htons function converts a u_short from host to TCP/IP network byte order ( which is big-endian ).
	

	//Place SOCKET & SOCKADDR_IN pointers addresses in local pointers
	*p_p_s_mainServerSocket = p_s_mainServerSocket;
	*p_p_service = p_service;



	//Bind the main Server socket to a local "socket address" assembled in the SOCKADDR_IN struct using Bind function (of Winsock2)
	// In order to use Bind the SOCKADDR_IN will be explicitley typecasted to SOCKADDR struct of the Same size
	bindResult = bind(*p_s_mainServerSocket, (SOCKADDR*)p_service, sizeof(*p_service));
	if (SOCKET_ERROR == bindResult) {
		printf("Error: Failed to bind an IP address & a port number to main Server socket using bind( ), with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		//Close & free the data of the Server's main(listening) socket & set its pointer's pointer to NULL
		closeSocketProcedure(p_s_mainServerSocket); //also frees dynamic memory
		*p_p_s_mainServerSocket = NULL;
		//Free the SOCKADDR_IN dynamic memory allocated & set its pointer's pointer to NULL
		free(p_service);
		*p_p_service = NULL;
		return STATUS_CODE_FAILURE;
	}

	// Listen on the Socket.
	listenResult = listen(*p_s_mainServerSocket, SOMAXCONN); //MIGHT NEED TO CHANGE TO 3 - BECAUSE IT WAS SAID THAT ONLY 3 PLAYERS (CLIENTS) ARE ALLOWED TO REQUEST CONNECTION TO THE SERVER
	if (SOCKET_ERROR == listenResult) {
		printf("Error: Failed listening on socket, with error code no. %ld.\n", WSAGetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		//Close & free the data of the Server's main(listening) socket & set its pointer's pointer to NULL
		closeSocketProcedure(p_s_mainServerSocket); //also frees dynamic memory
		p_p_s_mainServerSocket = NULL;
		//Free the SOCKADDR_IN dynamic memory allocated & set its pointer's pointer to NULL
		free(p_service);
		p_p_service = NULL;
		return STATUS_CODE_FAILURE;
	}

	

	
	//Memory allocation for a SOCKET struct & socket Creation, Binding, Listening have been successful
	return STATUS_CODE_SUCCESS;
}

static BOOL setServerListeningSocketInSocketsSetAndAdjustAcceptTimeout(SOCKET* p_s_serverSocket, fd_set** p_p_serverListeningSocketSet,struct timeval** p_p_clientsAcceptSelectTimeout)
{
	fd_set* p_socketSet = NULL;
	struct timeval* p_clientsAcceptSelectTimeout = NULL;
	//Assert
	assert(NULL != p_p_serverListeningSocketSet);
	assert(NULL != p_p_clientsAcceptSelectTimeout);
	

	//fd_set struct dynamic memory allocation
	if (NULL == (p_socketSet = (fd_set*)calloc(sizeof(fd_set), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a fd_set struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}

	//Clear set   -MIMGHT NEED TO CHECK FOR ERRORS
	FD_ZERO(p_socketSet); 
	//Add Server listening socket to the set
	FD_SET(*p_s_serverSocket, p_socketSet); 

	//timeval struct dynamic memory allocation
	if (NULL == (p_clientsAcceptSelectTimeout = (struct timeval*)calloc(sizeof(struct timeval), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a timeval struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_socketSet);
		return STATUS_CODE_FAILURE;
	}

	//Set timeval fields with the desired timeout values...
	p_clientsAcceptSelectTimeout->tv_sec = SELECT_TIMEOUT_SEC;
	p_clientsAcceptSelectTimeout->tv_usec = SELECT_TIMEOUT_MILLI_SEC;

	//Place fd_set & timeval pointers addresses in local pointers
	*p_p_serverListeningSocketSet = p_socketSet;					
	*p_p_clientsAcceptSelectTimeout = p_clientsAcceptSelectTimeout; 

	

	
	//The procedure of allocating dyanmic(Heap) memory for an fd_set and placing the Server's listening (main) socket in it &
	//	allocating dynamic(Heap) memory for an timeval struct and updating it with a predetermined Select function 
	//	(for an Accept function) timeout have been successful.
	return STATUS_CODE_SUCCESS;
}

static BOOL createThreadsHandlesAndTheirIds(HANDLE** p_p_h_clientsThreadsHandles, LPDWORD* p_p_threadIds, HANDLE** p_p_h_exitThread)
{
	HANDLE* p_h_clientsThreadsHandles = NULL, * p_h_exitThread = NULL;
	LPDWORD p_threadIds = NULL;
	//Assert
	assert(NULL != p_p_h_clientsThreadsHandles);
	assert(NULL != p_p_threadIds);
	assert(NULL != p_p_h_exitThread);

	//Allocate memory for threads handles - #Handles = #Threads
	if (NULL == (p_h_clientsThreadsHandles = (HANDLE*)calloc(sizeof(HANDLE), NUM_OF_WORKER_THREADS))) {
		printf("Error: Failed to allocate memory for the Connected Clients threads Handles array (in Server-side).\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}

	//Allocate memory for the Worker threads' IDs (threads the communicate with Connected(-to-Server) Clients)
	if (NULL == (p_threadIds = (LPDWORD)malloc(sizeof(DWORD) * NUM_OF_WORKER_THREADS))) {
		printf("Error: Failed to allocate memory for a Worker Threads Handles' IDs array.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
		free(p_h_clientsThreadsHandles);
		return STATUS_CODE_FAILURE;
	}

	//Allocate memory for the EXIT\ERROR thread handle 
	if (NULL == (p_h_exitThread = (HANDLE*)calloc(sizeof(HANDLE), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for the Exit/Error thread Handle (in Server-side).\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
		free(p_h_clientsThreadsHandles);
		free(p_threadIds);
		return STATUS_CODE_FAILURE;
	}

	//Update the input pointers addresses
	*p_p_h_clientsThreadsHandles = p_h_clientsThreadsHandles;//*p_p_h_clientsThreadsHandles = p_h_clientsThreadsHandles;
	*p_p_threadIds = p_threadIds;//*p_p_threadIds = p_threadIds;
	*p_p_h_exitThread = p_h_exitThread;//*p_p_h_exitThread = p_h_exitThread;

	//Allocating memory for all Worker threads handles and their IDs & the Exit\Error thread have been successful
	return STATUS_CODE_SUCCESS;
}

static workingThreadPackage** createSynchronousObjectsAndInsertTheirPointersToThreadPackages()
{
	workingThreadPackage** p_p_threadPackages = NULL;
	int i = 0;
	//HANDLE* p_h_gameSessionFileMutex = NULL, * p_h_firstPlayerEvent = NULL, * p_h_secondPlayerEvent = NULL;

	//workingthreadPackage struct pointers dynamic memory allocation
	if (NULL == (p_p_threadPackages = (workingThreadPackage**)calloc(sizeof(workingThreadPackage*), NUM_OF_WORKER_THREADS))) {
		printf("Error: Failed to allocate memory for workingthreadPackage struct pointers.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return NULL;
	}

	//0o0o0o0o0o0  Resource 1 0o0o0o0o0o0
	//Allocating dynamic memory (Heap) for the number of current connected Clients resource
	if (NULL == (g_p_currentNumOfConnectedClients = (USHORT*)calloc(sizeof(USHORT), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for the number of current connected Clients resource.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_p_threadPackages);
		return NULL;
	}

	//Allocating dynamic memory (Heap) for the number of current connected Clients resource Mutex Handle & Creating the Mutex and fetching the its handle's pointer 
	if (NULL == (g_p_h_connectedClientsNumMutex = allocateMemoryForHandleAndcreateMutex())) {
		printf("Error: Failed to allocate memory for the number of current connected Clients resource Mutex Handle & to create the Mutex object.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_p_threadPackages);
		free(g_p_currentNumOfConnectedClients);
		return  NULL;
	}

	


	//0o0o0o0o0o0  Resource 2 0o0o0o0o0o0
	//Allocating dynamic memory (Heap) for the GameSession.txt file resource First player entering Event Handle & Creating the Event and fetching its handle's pointer
	if (NULL == (g_p_h_firstPlayerEvent = allocateMemoryForHandleAndCreateEvent(
		AUTO_RESET,					/* if a first player of a couple attempted to create GameSession.txt  OR  to Read\Write from\to it, then un-signal the event */
		INITIALLY_SIGNALED,			/* the first player of a couple should be the FIRST to ....FILL */
		NULL))) {					/* un-named */
		printf("Error: Failed to allocate memory for the GameSession.txt file resource First Player enetering Event Handle & to create the Event object.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_p_threadPackages);
		free(g_p_currentNumOfConnectedClients);
		closeHandleProcedure(g_p_h_connectedClientsNumMutex);
		return  NULL;
	}

	//Allocating dynamic memory (Heap) for the GameSession.txt file resource Second player entering Event Handle & Creating the Event and fetching its handle's pointer
	if (NULL == (g_p_h_secondPlayerEvent = allocateMemoryForHandleAndCreateEvent(
		AUTO_RESET,					/* if a second player of a couple attempted to Read\Write from\to it, then un-signal the event */
		INITIALLY_NON_SIGNALED,		/* the second player of a couple should be the SECOND to ....FILL */ 
		NULL))) {					/* un-named */
		printf("Error: Failed to allocate memory for the GameSession.txt file resource Second Player enetering Event Handle & to create the Event object.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_p_threadPackages);
		free(g_p_currentNumOfConnectedClients);
		closeHandleProcedure(g_p_h_connectedClientsNumMutex);
		closeHandleProcedure(g_p_h_firstPlayerEvent);
		return  NULL;
	}



	//0o0o0o0o0o0  EXIT   ERROR   EVENTS 0o0o0o0o0o0
	//Allocating dynamic memory (Heap) for the "Exit" Event Handle & Creating the Event and fetching its handle's pointer
	if (NULL == (g_p_h_exitEvent = allocateMemoryForHandleAndCreateEvent(
		MANUAL_RESET,					/* when set to signal, this event should notify to ALL events they should finish and exit */
		INITIALLY_NON_SIGNALED,			/* only when Exit is inserted to STDin, this event should notify ALL Server threads */
		NULL))) {						/* special name! -> un-named */
		printf("Error: Failed to allocate memory for the 'exit' notifier Event Handle & to create the Event object.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_p_threadPackages);
		free(g_p_currentNumOfConnectedClients);
		closeHandleProcedure(g_p_h_connectedClientsNumMutex);
		closeHandleProcedure(g_p_h_firstPlayerEvent);
		closeHandleProcedure(g_p_h_secondPlayerEvent);
		return  NULL;
	}
	//Allocating dynamic memory (Heap) for the "Error" Event Handle & Creating the Event and fetching its handle's pointer
	if (NULL == (g_p_h_errorEvent = allocateMemoryForHandleAndCreateEvent(
		MANUAL_RESET,					/* when set to signal, this event should notify to ALL events they should finish and exit */
		INITIALLY_NON_SIGNALED,			/* only when an Error has occured, this event should notify ALL Server threads */
		NULL))) {						/* special name! -> un-named */
		printf("Error: Failed to allocate memory for the 'exit' notifier Event Handle & to create the Event object.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_p_threadPackages);
		free(g_p_currentNumOfConnectedClients);
		closeHandleProcedure(g_p_h_connectedClientsNumMutex);
		closeHandleProcedure(g_p_h_firstPlayerEvent);
		closeHandleProcedure(g_p_h_secondPlayerEvent);
		closeHandleProcedure(g_p_h_exitEvent);
		return  NULL;
	}

	for (i; i < NUM_OF_WORKER_THREADS; i++) {
		if (NULL == (*(p_p_threadPackages + i) = createThreadPackageAndInsertSynchronousObjectsPointerToThem()));
			//cLEAR MEMORY
	}

	//Worker thread inputs struct(package), the input for the thread in the Server that communicates with a Client, was created successfuly
	//	with all needed Synchronous objects' pointers
	return p_p_threadPackages;
	

}

static workingThreadPackage* createThreadPackageAndInsertSynchronousObjectsPointerToThem()
{
	workingThreadPackage* p_threadPackage = NULL;

	//workingthreadPackage struct dynamic memory allocation
	if (NULL == (p_threadPackage = (workingThreadPackage*)calloc(sizeof(workingThreadPackage), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a workingthreadPackage struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return NULL;
	}

	//Update the Working thread package struct's fields with ALL the needed pointers 
	p_threadPackage->p_currentNumOfConnectedClients = g_p_currentNumOfConnectedClients;
	p_threadPackage->p_h_connectedClientsNumMutex = g_p_h_connectedClientsNumMutex;
	p_threadPackage->p_h_firstPlayerEvent = g_p_h_firstPlayerEvent;
	p_threadPackage->p_h_secondPlayerEvent = g_p_h_secondPlayerEvent;
	p_threadPackage->p_h_exitEvent = g_p_h_exitEvent;
	p_threadPackage->p_h_errorEvent = g_p_h_errorEvent;


	//Worker thread inputs struct(package), the input for the thread in the Server that communicates with a Client, was created successfuly
	//	with all needed Synchronous objects' pointers
	return p_threadPackage;
}

static HANDLE* allocateMemoryForHandleAndcreateMutex()
{
	HANDLE* p_h_mutexHandle = NULL;
	//Allocating dynamic memory (Heap) for a Mutex Handle 
	if (NULL == (p_h_mutexHandle = (HANDLE*)calloc(sizeof(HANDLE), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a Mutex synchronous object.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return NULL;
	}

	//Creating an un-named Mutex (It is sufficent for it to be un-named, because the pointer to the handle will be transfered to all accessing threads)
	*(p_h_mutexHandle) = CreateMutex(
		NULL,		/* default security attributes */
		FALSE,		/* initially not owned */
		NULL);		/* unnamed mutex */
	
	//Validate the Mutex creation (The Mutex synchronous object is created un-named so there is no need to check ERROR_ALREADY_EXISTS case) 
	if (NULL == *(p_h_mutexHandle)) {
		printf("Error: Failed to create a Handle to a Mutex with code: %ld.\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_h_mutexHandle);
		return  NULL;
	}



	//Mutex creation was succesful
	return p_h_mutexHandle;
}

//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o 









//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0    Main Server thread's main loop functions 
//Alter name 'CHECK'
static int findFirstUnusedThreadSlot(HANDLE* p_h_clientsThreadsHandles)
{
	int t=0; //index
	DWORD result = 0;
	//Assert
	assert(NULL != p_h_clientsThreadsHandles);
	
	//Begin search for an idle thread position (reminder: There may be only 3 accessing Client per moment as instructed in Moodle)
	for (t ; t < NUM_OF_WORKER_THREADS; t++)
	{
		if (*(p_h_clientsThreadsHandles + t) == NULL)
			//A position of an idle thread was found
			break;
		else
		{
			// poll to check if thread finished running:
			switch (WaitForSingleObject(*(p_h_clientsThreadsHandles + t), SAMPLE)) {
			case WAIT_OBJECT_0:  // this thread finished running
				if (COMMUNICATION_FAILED == validateThreadExitCode(p_h_clientsThreadsHandles + t)) 
					if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)) { //Error event was probably already set //'FUNC'
						printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
						printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
						//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
						return STATUS_SERVER_ERROR;
					}
				CloseHandle(*(p_h_clientsThreadsHandles + t));
				*(p_h_clientsThreadsHandles + t) = NULL;
				break;
			case WAIT_TIMEOUT:
				//Thread is still running and is connected to a Client
				break;
			default:
				//e.g. WAIT_FAILED
				printf("Error: Failed to wait for Server-side Worker(Communication-with-Client) thread to finish properly using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)) { //'FUNC'
					printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
					printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
					//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
					return STATUS_SERVER_ERROR;
					break;
				}
			}
		}
	}
	//Return the earliest Idle Working thread (Client-to-Server thread) index   OR   NUM_OF_WORKER_THREADS if already 3 Clients are connected
	return t;
}

static int findIdleWorkerThreadForTheNewConnectedClientAndInitiate(SOCKET* p_s_acceptSocket/*Stack mem.*/, HANDLE* p_h_clientsThreadsHandles, LPDWORD p_threadIds, workingThreadPackage** p_p_threadPackages)
{
	int idleWorkingThreadIndex = 0;
	//Assert
	assert(NULL != p_s_acceptSocket);
	assert(NULL != p_h_clientsThreadsHandles);
	assert(NULL != p_threadIds);
	assert(NULL != p_p_threadPackages);


	// Find the index of the first idle Worker Server thread, to attach to newly connected Client
	idleWorkingThreadIndex = findFirstUnusedThreadSlot(p_h_clientsThreadsHandles);
	switch (idleWorkingThreadIndex){
	case NUM_OF_WORKER_THREADS: //no slot is available 
		printf("No slots available for client, dropping the connection.\n");
		//Closing the socket, dropping the connection & keep running the main loop...
		closeSocketProcedure(p_s_acceptSocket);
		break;
	
	case -1: //idleWorkingThreadIndex == STATUS_SERVER_ERROR -> one of FindFirstUnusedThreadSlot(.) failed
		closeSocketProcedure(p_s_acceptSocket);
		return STATUS_SERVER_ERROR;

	default: //0, 1 or 2 current connected Clients
		//There is an idle Worker Server thread that can operate the connection with the new Connected Client..
		//Insert the Accepted socket address into the Worker Server thread's inputs struct's relevant pointer
		(*(p_p_threadPackages + idleWorkingThreadIndex))->p_s_acceptSocket = p_s_acceptSocket;
		//Update the number of Current Connected(-to-Server) Clients by owning the resource's Mutex & incrementing the number
		if (MUTEX_CONTROL_FAILURE != incrementDecrementCheckNumberOfConnectedClients(g_p_currentNumOfConnectedClients, g_p_h_connectedClientsNumMutex, 1, CUR_CON_CLI_MUTEX_OWNERSHIP_TIMEOUT)) {
			/* ------------------------------------------------------------------------------------------------------------- */
			/*...............Initiate Server Worker thread to operate the communication with the newly connected Client		 */
			/* ------------------------------------------------------------------------------------------------------------- */
			if (INVALID_HANDLE_VALUE == (*(p_h_clientsThreadsHandles + idleWorkingThreadIndex) = createThreadSimple(
				(LPTHREAD_START_ROUTINE)serverSideWorkerThreadRoutine,	/* Server Worker thread routine func. */
				*(p_p_threadPackages + idleWorkingThreadIndex),			/* thread package (inputs) of the idle thread with smallest index */
				(p_threadIds + idleWorkingThreadIndex)))) {				/* address of of the thread ID */

				//Thread creation failed. Set "Exit"\"Error" Event to signal and begin exiting procedure..
				printf("Error: Failed to initiate a thread for the Server to communicate with the newly connected Client, with error code no. %ld.\nExiting..\n\n", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
				//Signal 'Error' Event....
				if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)) {//'FUNC'
					printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
					printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
					closesocket(*p_s_acceptSocket);
					//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
				}
				//We return a value that will cause the main Server thread's main loop to exit because an error occured when 
				// initializing a Worker thread. *PAY ATTENTION Exiting the main loop will occure even if the "Error" Event could not be signaled!!!!!! *PAY ATTENTION
				return STATUS_SERVER_ERROR;
			} 
			//Thread creation succeeded...
		}
		else {
			//Mutex ownership failed. 
			printf("Error: Failed to own the number of currently connected Clients Mutex, with error code no. %ld.\nExiting..\n\n", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
			//Signal "Error" Event & begin exiting procedure..
			if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)) { //'FUNC'
				printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n", __FILE__, __LINE__, __func__);
				closesocket(*p_s_acceptSocket);
				//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
			}
			//We return a value that will cause the main Server thread's main loop to exit because an error occured when 
			// initializing a Worker thread. *PAY ATTENTION Exiting the main loop will occure even if the "Error" Event could not be signaled!!!!!! *PAY ATTENTION
			return STATUS_SERVER_ERROR;
		}
		break;
	}
	//If Worker thread was initialized correctly or the incoming Client is currently a 4th connected client, then keep looping...
	return KEEP_GOING;
}

static int validateErrorExitEventsStatus(int exitFlag)
{
	switch (WaitForSingleObject(*g_p_h_errorEvent, SAMPLE)) {
	case WAIT_TIMEOUT:
		//"Error" Event wasn't signaled by an error occurring in any thread of Server process unless noted in exitFlag!!
		if (STATUS_SERVER_ERROR != exitFlag)
			switch (WaitForSingleObject(*g_p_h_exitEvent, SAMPLE)) {
				//either an 'exit' from STDin or
			case WAIT_TIMEOUT: break; //proceed... to KEEP_GOING
			case WAIT_OBJECT_0: return STATUS_SERVER_EXIT;
			default:
				printf("Error: Failed to sample Server-side 'EXIT' Event's status using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				//Signal 'Error' Event....
				if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)){  //Error event was probably already set 'FUNC'
					printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
					printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
					//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
				}

				return STATUS_SERVER_ERROR;
			}
		else  return STATUS_SERVER_ERROR; //Error has occured and signaling "Error" event failed!
		break;

	case WAIT_OBJECT_0:
		//Includes the cases:  WAIT_FAILED (It is an Event so WAIT_ABANDONED is irrelevant but is included regardless)
		//"Error" Event was signaled byan error that occured in the Server  SO  'exit' that may or may not inserted in STDin is ignored
		return STATUS_SERVER_ERROR; break;

	default:
		printf("Error: Failed to sample Server-side 'ERROR' Event's status using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		//Signal 'Error' Event....
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*g_p_h_errorEvent)) {  //Error event was probably already set
			printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
			//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
		}
		 
		return STATUS_SERVER_ERROR; break;
	}



	// Proceed with Server's operation....
	return KEEP_GOING;
}
//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o 







//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0    Server main thread's finishing functions 

static void threadTermination(HANDLE* p_h_threadHandle)
{
	//Assert
	assert(NULL != p_h_threadHandle);

	//THERE MAY BE THREADS THAT TERMINATED BUT findFirstUnusedThreadSlot(.) DID'T CHANGE THEIR HANDLE VALUE TO 'NULL' 
	if (NULL != *p_h_threadHandle) {
		switch (WaitForSingleObject(*p_h_threadHandle, SAMPLE)) { // This may be a second time a "Wait" function is activated on this thread, because WaitForMultipleObjects(.) wait for a GROUP of threads... 
		case WAIT_OBJECT_0: // Thread finished... NO NEED TO ENFORCE TERMINATION
			break;
		case WAIT_TIMEOUT: // Thread hasn't finished after timeout...  ENFORCE TERMINATION !
			if (STATUS_CODE_FAILURE == TerminateThread(*p_h_threadHandle, 0x555/*arbitrary new exit code which isn't 'success'*/)) {
				printf("Error: Failed to thread, with error code no. %ld.\n\n", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
			}
			break;
		default: //Ignore enforcing termination because thread status is not clear
			break;
		}
	}

	//"Error" will be transfered outward whether a thread termination succeeds or not
	//ignored atm - return STATUS_SERVER_ERROR;

}


static int threadsStatusValidationAndHandlingBeforeExiting(HANDLE* p_h_clientsThreadsHandles, HANDLE* p_h_exitThread, int timeout)
{
	int t = 0;
	BOOL waitRes = 0, retValGetExitCode=0;
	DWORD exitCode = 0;
	//Assert
	assert(NULL != p_h_clientsThreadsHandles);
	assert(NULL != p_h_exitThread);

	//If the first Client Worker thread handle has the value NULL then all Worker threads are inactive -> no need to wait for any of them!
	if (NULL != *p_h_clientsThreadsHandles) {
		waitRes = validateThreadsWaitCode(p_h_clientsThreadsHandles, *g_p_currentNumOfConnectedClients/*NUM_OF_WORKER_THREADS*/, timeout);
		// Wait for all threads to finish after "Exit" was inserted to server(.exe) process console
		//	CRITICAL OPERATION AHEAD - PAY ATTENTION - resource 1, the number of currently connected(to-Server) Clients is used as the WaitForMultipleObject thread count...
		//											   The reason for that is the this number is in fact the number of threads we wish the wait for.
		//											   The case where various "Context Switche"s between threads will occur until WaitForMultipleObject(.) is used, in which threads
		//											   will terminate on their own, because these threads will own the resource only in order to decrement it before TERMINATING...

		switch (waitRes) {
		case WAIT_TIMEOUT:
			//Terminate all threads
			for (t; t < NUM_OF_WORKER_THREADS; t++)
				//printf("%d\n", t);
				threadTermination(p_h_clientsThreadsHandles + t);
			break;

		case WAIT_OBJECT_0:
			// Validate the integrity of all threads exit codes (Exit thread, players threads) - 'CHECK' MIGHT NEED TO CHANGE exitCode approval 
			printf("All Server Worker threads completed on time & as desired!\nProceed to validate 'Exit' thread.....\n");
			for (t; t < NUM_OF_WORKER_THREADS; t++)
				if (STATUS_CODE_FAILURE == validateThreadExitCode(p_h_clientsThreadsHandles/*, *g_p_currentNumOfConnectedClients NUM_OF_WORKER_THREADS*/)) {
					return STATUS_SERVER_ERROR;
				}
			break; //Proceed to exit codes validation >>>>>>>>

		default:
			//WaitForMultipleObject(.) fails
			return STATUS_SERVER_ERROR;
		}
	}



	//Validate the termination of the STDin "Exit" thread
	switch (WaitForSingleObject(*p_h_exitThread, timeout)) {
	case WAIT_OBJECT_0:
		//Thread has terminated. validate its exit code
		retValGetExitCode = GetExitCodeThread(*p_h_exitThread, &exitCode);
		//Validate the thread terminated correctly
		if (GET_EXIT_CODE_FAILURE == retValGetExitCode) { //add case for a thread which is STILL_ACTIVE
			printf("Error when getting thread no. CHANGE exit code, with code: %d\n", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
			return STATUS_CODE_FAILURE;
		}
		if (STATUS_CODE_FAILURE == exitCode)  //'CHECK' MIGHT NEED TO CHANGE exitCode approval
			return STATUS_SERVER_ERROR;
		//Successful case, where no error was detected until Server process USER entered "exit" to STDin..........
		return STATUS_SERVER_EXIT; // Wrong case when exitFlag == -1 'CHECK'
		break;

	default:
		printf("Error: Failed to wait for 'Exit' thread to finish properly using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		//Thread hasn't terminated. Critical termination!!!
		if (STATUS_CODE_FAILURE == TerminateThread(*p_h_exitThread, 0x555/*arbitrary new exit code which isn't 'success'*/)) {
			printf("Error: Failed to terminate Worker thread no. %d, with error code no. %ld.\n\n", t, GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		}
		return STATUS_SERVER_ERROR;
	}

}

static BOOL cleanupAndExit(int exitFlag,
	SOCKET* p_s_mainServerSocket,
	SOCKADDR_IN* p_service,
	fd_set* p_serverListeningSocketSet,
	struct timeval* p_clientsAcceptSelectTimeout,
	HANDLE* p_h_clientsThreadsHandles,
	LPDWORD p_threadIds,
	HANDLE* p_h_exitThread,
	workingThreadPackage** p_p_threadPackages)
{
	printf("exit Flag %d\n\n", exitFlag);
	switch (exitFlag) {
	case -1: // == STATUS_SERVER_ERROR
		//After notifying all existing threads, countdown begins... 
		//When countdown timesout, all threads will be terminated & all Handles, Sockets, Heap allocated memmory will be freed
		exitFlag = threadsStatusValidationAndHandlingBeforeExiting(p_h_clientsThreadsHandles, p_h_exitThread, ERROR_CASE_THREADS_TIMEOUT);
		break;
		
	case -2: // == STATUS_SERVER_EXIT
		//Close all Handles & Sockets and Free dyanmic allocated memory (resources)
		exitFlag = threadsStatusValidationAndHandlingBeforeExiting(p_h_clientsThreadsHandles, p_h_exitThread, EXIT_CASE_THREADS_TIMEOUT);
		break;
	}	

	//Clean Synchronous objects & Close their Handles & Free the Worker threads inputs structs memory
	freeTheWorkingThreadPackages(p_p_threadPackages);
	//Clean the Threads allocated Handles' dynamic memory
	closeThreadsProcedure(p_h_clientsThreadsHandles, p_threadIds, NUM_OF_WORKER_THREADS);
	closeThreadsProcedure(p_h_exitThread, NULL, SINGLE_OBJECT);
	//Clean the Listening Socket's associate data structures 
	closeListeningSocketProcedure(p_s_mainServerSocket, p_service, p_serverListeningSocketSet, p_clientsAcceptSelectTimeout);

	// Clean Winsock2
	if (SOCKET_ERROR == WSACleanup()) {
		printf("Error: Failed to close Winsocket, with error code no. %ld.\nExiting...\n\n", WSAGetLastError());
		return STATUS_CODE_FAILURE;
	}
	//Server operation failed... // Server managed Game Room successfully
	return (exitFlag == STATUS_SERVER_ERROR) ? STATUS_CODE_FAILURE : STATUS_CODE_SUCCESS;
}
