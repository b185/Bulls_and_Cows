/* ServerSideWorkerThreadRoutine.c
-------------------------------------------------------------------------------------
	Module Description - This module contains the Server-side Worker thread routine
		and almost all functions that assist the thread routine to communicate
		with a Client-side Speaker thread.
-------------------------------------------------------------------------------------
*/

// 'CHECK' 'DELETE'

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
#include "MessagesTransferringTools.h"
#include "FilesHandlingTools.h"



// Constants --------------------------------------------------------------------
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;

static const int SAMPLE = 0;
static const int SINGLE_OBJECT = 1;

//Duration constants
static const int SHORT_SERVER_RESPONSE_WAITING_TIMEOUT = 15000;	// 15 Seconds
static const int LONG_SERVER_RESPONSE_WAITING_TIMEOUT = 600000; // 600 Seconds = 10 Min
static const int KEEP_RECEIVE_TIMEOUT = -1;
static const long CUR_CON_CLI_MUTEX_OWNERSHIP_TIMEOUT = 2000; // 2 Seconds - Current Connected Clients count timeout
static const long GAME_SESSION_FILE_MUTEX_OWNERSHIP_TIMEOUT = 2200; // 2.2 Seconds - GameSession.txt timeout



static const int COPY_OPPONENT_NAME_FAILED = -1;
static const BOOL MUTEX_OWNERSHIP_RELEASE_FAILED = 0;

//0o0o0o Server process closure status constants
//static const BOOL KEEP_GOING = 0;
//static const BOOL STATUS_SERVER_ERROR = -1;
//static const BOOL STATUS_SERVER_EXIT = -2;

// Global variables ------------------------------------------------------------
//Player disconnection bit
int g_serverOpponentQuitBit = 0;

// Functions declerations ------------------------------------------------------

/// <summary>
/// Description - This function accesses the "Currently Connected Clients Number" resource. It does so for either incrementing the number,
///	 decrementing the number when a Client leaves, or checks its value. The addition of this function on top of the functionality of
/// incrementDecrementCheckNumberOfConnectedClients(.) is that it also signals the Server's "ERROR" event if a FATAL error has occured, and by doing so,
/// it leads to the termination of the Server process
/// </summary>
/// <param name="workingThreadPackage* p_params - the thread's inputs package(struct)"></param>
/// <param name="int increDecre - increment\decrement\fetch bit"></param>
/// <returns>the updated number of currently connected clients if successful, or -1 after signaling the 'ERROR' Event</returns>
static int incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(workingThreadPackage* p_params, int increDecre);

/// <summary>
/// Description - This function samples the statuses of the 'ERROR' and 'EXIT' events, and performs accordingly, meaning 
/// it outputts the either an Exit, Error, Keep going bit. for Exit\Error the thread will initiate termination, and for Keep going the thread will continue
/// </summary>
/// <param name="HANDLE* p_h_errorEvent - a pointer to the handle of the 'ERROR' event"></param>
/// <param name="HANDLE* p_h_exitEvent - a pointer to the handle of the 'EXIT' event"></param>
/// <returns>STATUS_SERVER_EXIT if 'EXIT' Event is signaled. STATUS_SERVER_ERROR if 'ERROR' is signaled (even if 'EXIT' is signaled). KEEP GOING if none is signaled</returns>
static BOOL validateErrorExitEventsStatusInWorkerThread(HANDLE* p_h_errorEvent, HANDLE* p_h_exitEvent);


/// <summary>
/// Description - This function begins by expecting (attempting receiving) CLIENT_REQUEST, then performs first 'ERROR'\'EXIT' events statuses checks***, 
/// then responds to CLIENT_REQUEST and finally perform a second 'ERROR'\'EXIT' events statuses check***
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' code according to all of the Exit codes possible</returns>
static communicationResults decideIfNewlyConnectedClientIsThirdPlayerAndApproveOrDeclineConnection(workingThreadPackage* p_params);
/// <summary>
/// Description - This function responds to the Client's received CLIENT_REQUEST with either SERVER_DENIED or SERVER_APPROVED. In the early the Server
/// attempt 'Graceful disconnect' from the Client and terminates the Worker thread. In the latter, the Server progresses the communication to SERVER MAIN Menu
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' code according to all of the Exit codes possible</returns>
static communicationResults responseToClientRequestMessage(workingThreadPackage* p_params);



/// <summary>
/// Description - This function progress from the point CLIENT_REQUEST was APPROVED. It sends to the Client  SERVER_MAIN_MENU, and awaits response. If the reponse is
/// CLIENT_VERSUS, (Here it will check 'ERROR'\'EXIT' events statuses***) and the thread noticed there is more than one user connected, the function will proceed to 
/// Synchronize between the two players, who  desire to play, by using the 1st aand 2nd Player events.... after synchronizing them, the function will assist 
/// the Worker threads to exchange the players names, and then will proceed to the "Game Room", there a CLIENT_INVITE will be sent... 
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' code according to all of the Exit codes possible</returns>
static communicationResults initiateMainMenuProcedure(workingThreadPackage* p_params);

/// <summary>
/// Description - This function "listen" to receive the Clients response to  SERVER_MAIN_MENU, which can be either  CLIENT_VERSUS, which means the Client
/// wants to play "Cows and Bulls", or   CLIENT_DISCONNECT, which means the Client chooses to leave connection and will disconnect and be channeled to "Connection menu"
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' code according to all of the Exit codes possible</returns>
static communicationResults mainMenuClientResponses(workingThreadPackage* p_params);
/// <summary>
/// Description - This function mainly vaildates the availability of another player to play a game. It first checks the "Current Connected Clients Number", if it equals 1
/// then the function will send back to the user the SERVER_NO_OPPONENTS message, and loop back to SERVER_MAIN_MENU (along with sending main menu message). If there are two players
/// currently connected, the function will perform the Sync between them (under the assumption both will player against one another). Following that another ERROR'\'EXIT' events statuses
/// check is performed***. (if the number of Curre Conn Clie is -1, the Mutex ownership has probably failed -> that is a fatal error and the thread will leave with COMMUNICATION_FAILED)
/// If Syncing the players takes too long (10 min) the player that had already sent CLIENT_VERSUS, will be outputted back to main menu with SERVER_NO_OPPONENTS
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' codes - COMM_SUCCEEDED, FAILED,   SERVER DISCONNECT, BACK TO MENU, SERVER EXIT etc.</returns>
static communicationResults validateThereAreTwoPlayersInGameRoomAndSynchronizeThem(workingThreadPackage* p_params);
/// <summary>
/// Description - This function receives either the "1st Player" or "2nd Player" events and attempts to set them to signaled with a signaling of the 'ERROR' event
/// if SetEvent(.) fails = fatal error
/// </summary>
/// <param name="HANDLE* p_h_playerEvent - 1st or 2nd Player events"></param>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <param name="int playerId - indictor for printf"></param>
/// <returns>True if signaling succeeded. FALSE if failed</returns>
static BOOL setPlayerEventToSignaled(HANDLE* p_h_playerEvent, workingThreadPackage* p_params, int playerId);

/// <summary>
/// Description - IMPORTANT FUNCTION - this is the main function responsible over the Synchronization of the two playing Worker threads when attempting to access
/// the GameSession.txt file resource for the purpose of exchanging data (e.g players names, initial numbers, guesses numbers). We follow the "Steps accessing" procedure:
/// 1st Player Event is initally set to signaled and 2nd Player event is set to non-signaled. First player ARRIVER is signals off the 1st event, and gets stuck on the
/// 2nd event. Then the Second player ARRIVER passes through both events and performs the FIRST WRITE, and is considered, for that reason, the first player of this accessing round.
/// Then the second arriver signals on the 2nd event and GETS STUCK on the 1st event. This leads for the first arriver to get loose and access the file for "READ-then-Write", for that
/// reason he is named the second player of this round (first ARRIVER == 2nd player). Then the first arriver signals on the 1st event and GET STUCK on the 2nd event. This cause
/// the second arriver to get loose and access the file for a second time -> for READing.. Then the second arriver signals on the 2nd event. 
/// At this moment, both players exchanges their data, and the two players events are non-signaled, so lastly the first arriver signals the 1st event a SECOND time,
/// which results in returning to the original circumstances, and allows the same procedure for the following GameSession.txt data exchanges...
///  (the helper function is stepsStyleAccessingGameSessionFile(.)) 
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <param name="int dataType - the data type indicator on the type of data to be exchanged"></param>
/// <param name="int clientAbsencyMessageType - here will also be a place to exit with SERVER_NO_OPPONENTS or SERVER_OPPONENT_QUIT"></param>
/// <returns>'communicationResults' code indicating different results for this function, mainly fail(fatal error) or succeed</returns>
static communicationResults awaitBothPlayersByMarkingFirstClientThenSecondClient(workingThreadPackage* p_params, int dataType, int clientAbsencyMessageType);
/// <summary>
/// Description - IMPORTANT FUNCTION - this is the helper function to awaitBothPlayersByMarkingFirstClientThenSecondClient(.) and it mainly performs the description
/// above with when receiving the relevant data and is suitable to CREATE a GameSession.txt file also...
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <param name="int dataType - the data type indicator on the type of data to be exchanged"></param>
/// <param name="int firstPlayerBit - this bit describes the FIRST ARRIVER if it equals 1, and SECOND ARRIVER if equals 0"></param>
/// <param name="int writeBit - don't care for SECOND ARRIVER because he accesses the file once. with it FIRST ARRIVER know when to write and when to read"></param>
/// <param name="HANDLE* p_h_stuckEvent - pointer to the handle of the event the player will gets STUCK on"></param>
/// <param name="HANDLE*p_h_releaseEvent - pointer to the handle of the event the player will RELEASE to allow the other player to proceed to his next STEP"></param>
/// <returns>'communicationResults' code indicating different results for this function, mainly fail(fatal error) or succeed</returns>
static communicationResults stepsStyleAccessingGameSessionFile(workingThreadPackage* p_params, int dataTypeBit, int firstPlayerBit, int writeBit, HANDLE* p_h_stuckEvent, HANDLE* p_h_releaseEvent);






/// <summary>
/// Description - This function sends SERVER_INVITE and SERVER_SETUP_REQUEST, awaits response (initial numbers of the players), syncing between the Worker threads
/// of the Clients to exchange the players initial numbers, checks the status of the 'EXIT' and 'ERROR' events***, and procceds receive the players guesses until
/// the game reaches its end
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' codes - COMM_SUCCEEDED, FAILED,   SERVER DISCONNECT, BACK TO MENU, SERVER EXIT etc.</returns>
static communicationResults beginGame(workingThreadPackage* p_params);

/// <summary>
/// Desciption - This function performs the sending of SERVER_INVITE  and SERVER_SETUP_REQUEST,   while allowing between them to notice an abrupt disconnection 
/// of the OTHER Client, and send SERVER_OPPONENT_QUIT INSTEAD, and leave back to menu (currently broken) then it awaits CLIENT_SETUP which will tranfer the initial
/// Users numbers to the Server Worker threads...
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' codes - COMM_SUCCEEDED, FAILED,   SERVER DISCONNECT, BACK TO MENU, SERVER EXIT etc.</returns>
static communicationResults receiveInitialPlayerNumber(workingThreadPackage* p_params);
/// <summary>
/// Description - this function utilizes awaitBothPlayersByMarkingFirstClientThenSecondClient(.) and the "Steps Syncing" procedure to exchange the received 
/// initial Users numbers. Afterwards it perform another 'EXIT' and 'ERROR' events statu check***.
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <param name="int dataTypeBit - same data type indicator - will be set to 2 here for initial number transfer"></param>
/// <returns>'communicationResults' codes - COMM_SUCCEEDED, FAILED,   SERVER DISCONNECT, BACK TO MENU, SERVER EXIT etc.</returns>
static communicationResults syncPlayersAndTransferNumbers(workingThreadPackage* p_params, int dataTypeBit);

/// <summary>
/// Description - This function will  start by first sending either SERVER_OPPONENT_QUIT (if other player abruptly disconnected) or SERVER_PLAYER_MOVE_REQUEST if
/// gameing procedure go as planned to receive the player guess numbers (at every round). Then it will await the Client response with the guess number -> 
/// CLIENT_PLAYER_MOVE. Following the numbers arrival  at the Worker threads, the function will perform another "Steps Sync" procedure to exchanges the guess numbers.
/// Finally, it will proceed to compute the results of the current round - Win, Tie, Proceed		
/// (currently SERVER_OPPONENT_QUIT which leads to PLAYER_DISCONNECT is broken)
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' codes - COMM_SUCCEEDED, FAILED,   SERVER DISCONNECT, BACK TO MENU, SERVER EXIT etc.</returns>
static communicationResults receivePlayersGuessesAndComputeResults(workingThreadPackage* p_params);

/// <summary>
/// Description - This function fetches the "Bulls and Cows" round result aand sends the appropriate message to  the players. It proceeds
/// to either Server Main Menu, another guess round, complete termination etc... (currently SERVER_OPPONENT_QUIT which leads to PLAYER_DISCONNECT is broken)
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' codes - COMM_SUCCEEDED, FAILED,   SERVER PLAYER DISCONNECT, BACK TO MENU etc.</returns>
static communicationResults prepareResultsOfCurrentRoundAndSend(workingThreadPackage* p_params);


/// <summary>
///  Description - function to send a SERVER_DRAW messaage with the desired parameters, free the player no-longer needed data and leave...
/// (currently SERVER_OPPONENT_QUIT which leads to PLAYER_DISCONNECT is broken)
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <returns>'communicationResults' codes - COMM_SUCCEEDED, FAILED,   SERVER PLAYER DISCONNECT, BACK TO MENU etc.</returns>
static communicationResults sendDraw(workingThreadPackage* p_params);
/// <summary>
///  Description - function to send a SERVER_WIN messaage with the desired parameters, free the player no-longer needed data and leave...
/// (currently SERVER_OPPONENT_QUIT which leads to PLAYER_DISCONNECT is broken)
/// </summary>
/// <param name="workingThreadPackage* p_params - thread's inputs (pointers to Event objects, Mutex object, players data items, Socket)"></param>
/// <param name="char* p_winner - pointer to the winner name buffer"></param>
/// <returns>'communicationResults' codes - COMM_SUCCEEDED, FAILED,   SERVER PLAYER DISCONNECT, BACK TO MENU etc.</returns>
static communicationResults sendWinner(workingThreadPackage* p_params, char* p_winner);

/// <summary>
/// Description - This function utilizes concatenateStringToStringThatMayContainNullCharacters(.) to perform a BRUTE FORCE copy of the data transferred
/// </summary>
/// <param name="message* p_receivedMessageFromClient - the messag struct of the Client response"></param>
/// <param name="char** p_p_playerDataInMessage - some parameter address pointer buffer"></param>
/// <returns>0 if successful, -1 if failed</returns>
static int copyPlayerNameOrFourDigitNumberString(message* p_receivedMessageFromClient, char** p_p_playerNameInMessage);
/// <summary>
/// Description - This function conducts a single classic round of "Bulls and Cows" by comparing the number(string representation)
/// </summary>
/// <param name="TCHAR* p_opponentInitialDigits - pointer to buffer containing an initial number of one player"></param>
/// <param name="TCHAR*p_selfPlayerGuessDigits - pointer to buffer containing an guess number of the other player"></param>
/// <param name="SHORT* p_cowsAddress - address of the number of cows"></param>
/// <param name="SHORT* p_bullsAddress - address of the number of bulls"></param>
static void playSingleGamePhase(TCHAR* p_opponentInitialDigits, TCHAR* p_selfPlayerGuessDigits, SHORT* p_cowsAddress, SHORT* p_bullsAddress);


// Functions definitions -------------------------------------------------------

/*.... Exit codes:
	Bad operation - 0
	Correct operation - 1
	Timeout occured - 2
	Disconnection of Server - 3
	Disconnection of other player - 4
	Server denied the Client's connection - 5
	
*/
communicationResults WINAPI serverSideWorkerThreadRoutine(LPVOID lpParam)
{
	workingThreadPackage* p_params = NULL;
	char* p_recvdMessage = NULL;

	//Check whether lpParam is NULL - Input integrity validation
	if (NULL == lpParam) return EMPTY_THREAD_PARAMETERS;
	//Parameters input conversion from void pointer to section struct pointer by explicit type casting
	p_params = (workingThreadPackage*)lpParam;
	

	//Expect Connected Client's CLIENT_REQUEST message, and respond with either SERVER_DENIED, or SERVER_APPROVED followed by SERVER_MAIN_MENU
	switch (decideIfNewlyConnectedClientIsThirdPlayerAndApproveOrDeclineConnection(p_params)) {
	
	case COMMUNICATION_FAILED: /*CLOSING THREAD - CLIENT LEAVES*/
		freeThePlayer(p_params);	 //Free the Worker thread players parameters
		incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/);
		SetEvent(*(p_params->p_h_errorEvent));  //reason: various fatal error may have occured
		return COMMUNICATION_FAILED; break;

	case COMMUNICATION_EXIT: /*CLOSING THREAD - CLIENT LEAVES*/
		freeThePlayer(p_params);	//Free the Worker thread players parameters
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED;
		return COMMUNICATION_EXIT; break;

	case GRACEFUL_DISCONNECT: /*CLOSING THREAD - CLIENT LEAVES*/
			//Free the Worker thread players parameters
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED;
		return GRACEFUL_DISCONNECT; break;

	case COMMUNICATION_TIMEOUT: /*CLOSING THREAD - CLIENT LEAVES*/
		printf("Reached timeout on CLIENT_REQUEST\n"); //Client name was not attained
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED;
		return COMMUNICATION_TIMEOUT; break;

	case SERVER_DISCONNECTED: /*CLOSING THREAD - CLIENT LEAVES*/
		printf("Client disconnected\n");
		freeThePlayer(p_params);	//Free the Worker thread players parameters
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED;
		return SERVER_DISCONNECTED; break;

	case SERVER_DENIED_COMM: /*CLOSING THREAD - CLIENT LEAVES*/
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED;
		return SERVER_DENIED_COMM; break;

	default:// COMMUNICATION_SUCCEEDED: 
		break; // Proceed...>>>>
	}

	//>>>>
	//Client currently choosing from the following menu:
	//				Choose what to do next :
	//				1. Play against another client
	//				2. Quit
	//				Type 1 or 2
	//Awaiting human response, which means the Worker thread must wait for at least 10 minutes.
	//Following the Client's User response, a GameSession.txt will be initialized (Created\Opened & fed with player's name) & 
	switch (initiateMainMenuProcedure(p_params)) {
	case COMMUNICATION_FAILED: 
		freeThePlayer(p_params); //Free the Worker thread players parameters
		incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/); //Thread Terminates
		SetEvent(*(p_params->p_h_errorEvent));  //reason: various fatal error may have occured
		printf("COMMUNICATION_FAILED\n");
		return COMMUNICATION_FAILED; break;

	case COMMUNICATION_EXIT: 
		freeThePlayer(p_params); //Free the Worker thread players parameters
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED; //Thread Terminates
		printf("COMMUNICATION_EXIT\n");
		return COMMUNICATION_EXIT; break;

	case COMMUNICATION_TIMEOUT:
		freeThePlayer(p_params); //Free the Worker thread players parameters
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED; //Thread Terminates
		printf("COMMUNICATION_TIMEOUT\n");
		return COMMUNICATION_TIMEOUT; break;

	case SERVER_DISCONNECTED: 
		freeThePlayer(p_params); //Free the Worker thread players parameters
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED; //Thread Terminates
		 printf("SERVER_DISCONNECTED\n"); 
		 return SERVER_DISCONNECTED; break;

	case PLAYER_DISCONNECTED: //Client messaged CLIENT_DISCONNECT
		freeThePlayer(p_params); //Free the Worker thread players parameters
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED; //Thread Terminatesreturn 
		printf("PLAYER_DISCONNECTED\n");
		return PLAYER_DISCONNECTED; break;
	
	case GRACEFUL_DISCONNECT:
		freeThePlayer(p_params); //Free the Worker thread players parameters
		if (-1 == incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, -1/*decrement*/)) return COMMUNICATION_FAILED; //Thread Terminatesreturn PLAYER_DISCONNECTED; break
		printf("GRACEFUL_DISCONNECT\n");
		return GRACEFUL_DISCONNECT;

	default:// COMMUNICATION_SUCCEEDED: 
		freeThePlayer(p_params);
		//gracefulDisconnect()?
		//Communication finished properly.....
		printf("COMMUNICATION_SUCCEEDED\n");
		return COMMUNICATION_SUCCEEDED;
	}



}




//......................................Static functions..........................................

static int incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(workingThreadPackage* p_params, int increDecre)
{
	int extractValueOfNumberOfCurrentlyConnectedClients = 0;
	//Assert
	assert(NULL != p_params);
	assert((-1 <= increDecre) && (1 >= increDecre));

	//Fetch the number of currently connected Clients
	extractValueOfNumberOfCurrentlyConnectedClients = incrementDecrementCheckNumberOfConnectedClients(
		p_params->p_currentNumOfConnectedClients,	/* a pointer to the resource */
		p_params->p_h_connectedClientsNumMutex,		/* a pointer to the Handle of the Mutex to the resource */
		increDecre,									/* this is the value to increment\decrement. val=0 means no change will be applied to the resource's value */
		CUR_CON_CLI_MUTEX_OWNERSHIP_TIMEOUT);		/* maximal waiting duration. set to 2sec */
	
		//Validate the return value of the resource value retrival operation...
	switch (extractValueOfNumberOfCurrentlyConnectedClients) {
	case -1:
		//Error occured while attempting to own\release the Current Connected Clients Mutex
		gracefulDisconnect(p_params->p_s_acceptSocket); // no need to check return code, because general operation failed
		//Set 'ERROR' event due to error that just occured HERE!
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: Mutex ownership\release failed
			printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
		}
		return -1; break; // not communication timeout

	default:
		//Return the number of currently connection Clients
		return extractValueOfNumberOfCurrentlyConnectedClients;
	}
}

static BOOL validateErrorExitEventsStatusInWorkerThread(HANDLE* p_h_errorEvent, HANDLE* p_h_exitEvent)
{
	switch (WaitForSingleObject(*p_h_errorEvent, SAMPLE)) {
	case WAIT_TIMEOUT:
		//"Error" Event wasn't signaled by an error occurring in any thread of Server process unless noted in exitFlag!!
		switch (WaitForSingleObject(*p_h_exitEvent, SAMPLE)) {
			//either an 'exit' from STDin or
		case WAIT_TIMEOUT: return KEEP_GOING; break; //proceed... to KEEP_GOING

		case WAIT_OBJECT_0:
			printf("'EXIT' event indicates an 'exit' had been entered via STDin...\nThread no. %ld exiting...\n", GetCurrentThreadId());  //'DELETE' DEBUG MESSAGE
			return STATUS_SERVER_EXIT; break;

		default:
			printf("Error: Failed to sample Server-side 'EXIT' Event's status using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			//Signal 'Error' Event....
			if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*p_h_errorEvent)) {  //Error event was probably already set 'FUNC'
				printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			}

			return STATUS_SERVER_ERROR;

		}// switch (WaitForSingleObject(*p_h_exitEvent, SAMPLE))
		break;

	case WAIT_OBJECT_0:
		//"Error" Event was signaled by an error that occured in the Server 
		printf("'ERROR' event indicates an error has occured...\nThread no. %ld exiting...\n", GetCurrentThreadId());  //'DELETE' DEBUG MESSAGE
		return STATUS_SERVER_ERROR; break;

	default:
		printf("Error: Failed to sample Server-side 'ERROR' Event's status using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		//Signal 'Error' Event....
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*p_h_errorEvent)) {  //Error event was probably already set
			printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
		}

		return STATUS_SERVER_ERROR; break;

	}// switch (WaitForSingleObject(*p_h_errorEvent, SAMPLE))	
}




static communicationResults decideIfNewlyConnectedClientIsThirdPlayerAndApproveOrDeclineConnection(workingThreadPackage* p_params)
{
	message* p_receivedMessageFromClient = NULL;
	transferResults tranRes = 0;
	//USHORT extractValueOfNumberOfCurrentlyConnectedClients = 0;
	//Assert
	assert(NULL != p_params);

	//Firstly, receive the "CLIENT_REQUEST" message from the newly connected peer (Client) & store the player's name
	//	Proceed to block this Server Worker thread for the Client's message for 15 sec (set the receive timeout duration to 15 seconds)
	tranRes = receiveMessage(p_params->p_s_acceptSocket, &p_receivedMessageFromClient, SHORT_SERVER_RESPONSE_WAITING_TIMEOUT);
	if (TRANSFER_SUCCEEDED == tranRes) //Validate the receive operation result...
		switch (p_receivedMessageFromClient->messageType) {
		case CLIENT_REQUEST_NUM:
			//Copy the newly connected peer's name..
			if (COPY_OPPONENT_NAME_FAILED == copyPlayerNameOrFourDigitNumberString(p_receivedMessageFromClient, &p_params->p_selfPlayerName)) {
				freeTheMessage(p_receivedMessageFromClient);
				if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: Mem alloc failed
					printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
					printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
					//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
				}
				//Notify Client Speaker thread, that his connection, a Worker thread, disconnects due to faulty
				gracefulDisconnect(p_params->p_s_acceptSocket);
				return COMMUNICATION_FAILED;
			}
			//Free the received message arranged in a 'message' struct
			freeTheMessage(p_receivedMessageFromClient);
			//Continue... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			break;

		default: //Received a wrong message /* no other message is expected from the Server at this point */
			printf("Recived an unexpected message. Exiting\n");
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			freeTheMessage(p_receivedMessageFromClient);
			gracefulDisconnect(p_params->p_s_acceptSocket); //'CHECK TIMEOUT CASE'
			return COMMUNICATION_FAILED; break;
		}
	else {
		// The communication between the Server Worker thread & the Client Speaker thread failed during recv(.) function, 
		//   and transRes contains the reason which is also the thread's exit code IN THIS CASE
		if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		return (communicationResults)tranRes; //DON'T SET 'ERROR' EVENT!!!
	}

	//>>>>
	//1st -Check the status of the "ERROR" & "EXIT" events to know if it is needed to end the current Worker thread
	switch (validateErrorExitEventsStatusInWorkerThread(p_params->p_h_errorEvent, p_params->p_h_exitEvent)) {
	case STATUS_SERVER_EXIT: //Close all resources of the current Worker thread e.g. player name & exit thread
		if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		//free(g_p_selfPlayerName); 'CHECK'
		return COMMUNICATION_EXIT;

	case STATUS_SERVER_ERROR://Close all resources of the current Worker thread e.g. player name & exit thread
		if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		//free(g_p_selfPlayerName); 'CHECK'
		return COMMUNICATION_EXIT;

	default: // KEEP_GOING (0)
		break; //Proceed.........>>>>
	}





	//>>>>
	//Answer to the Client (its Speaker thread) by sending back SERVER_APPROVED or SERVER_DENIED
	switch (responseToClientRequestMessage(p_params)) {
	case SERVER_DENIED_COMM: //Sent ^ SERVER_DENIED ^ 
		printf("Declining Client speaking with Server Worker thread no. %ld\n", GetCurrentThreadId()); //'DELETE' DEBUG MESSAGE
		free(p_params->p_selfPlayerName);
		return SERVER_DENIED_COMM; break;

	case COMMUNICATION_SUCCEEDED: //Sent ^ SERVER_APPROVED ^  &  ^ SERVER_MAIN_MENU ^
		printf("Client speaking with Server Worker thread no. %ld is APPROVED by SERVER into (Game Room) Main Menu\n", GetCurrentThreadId()); //'DELETE' DEBUG MESSAGE
		break; //Proceed.........>>>>

	case SERVER_DISCONNECTED:
		//send(.) failed maybe due to disconnection....
		break;

	default: //COMMUNICATION_FAILED
		printf("Error has occured at thread no. %ld   decideIfNewlyConnectedClientIsThirdPlayerAndApproveOrDeclineConnection\n", GetCurrentThreadId());
		return COMMUNICATION_FAILED; break;
	}


	//>>>>
	//2nd - Check the status of the "ERROR" & "EXIT" events to know if it is needed to end the current Worker thread
	switch (validateErrorExitEventsStatusInWorkerThread(p_params->p_h_errorEvent, p_params->p_h_exitEvent)) {
	case KEEP_GOING: return COMMUNICATION_SUCCEEDED; break; //Proceed.........>>>>

	case STATUS_SERVER_ERROR://Close all resources of the current Worker thread e.g. player name & exit thread
		if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		//free(g_p_selfPlayerName);
		return COMMUNICATION_EXIT;

	default: //STATUS_SERVER_EXIT (-2)
		if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		//free(g_p_selfPlayerName);
		return COMMUNICATION_EXIT;
	}

}

static communicationResults responseToClientRequestMessage(workingThreadPackage* p_params)
{
	USHORT extractValueOfNumberOfCurrentlyConnectedClients = 0;
	communicationResults sendRes = 0;
	//Assert
	assert(NULL != p_params);

	//Lock the Number of currently connected Clients' Mutex in order to validate if the current Client is a THIRD CONNECTED CLIENT
	//	If it is, then terminate TCP connection with Client by first sending it SERVER_DENIED. Otherwise, send it SERVER_APPROVED...

	//Also, Release the ownership over the Mutex. It is okay to release it right now because, it was not defined when exactly a player is
	// considered a THIRD player compared to the other TWO players currently connected (probably playing). So if one of the two players,
	// disconnects following the release of the mutex by the third player, the third player is still considered a THIRD PLAYER, and is DECLINED from the connection 
	extractValueOfNumberOfCurrentlyConnectedClients = incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, 0/*fetch value*/);
	if(-1 == extractValueOfNumberOfCurrentlyConnectedClients)
		return COMMUNICATION_FAILED;  // not communication timeout
	else if (3 == extractValueOfNumberOfCurrentlyConnectedClients) {
		// The current Client is a THIRD client - Deny <<<<3rd 3rd 3rd
		
		//Send  $$$ ^ SERVER_DENIED ^  $$$
		
		if ((communicationResults)TRANSFER_SUCCEEDED == (sendRes= sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_DENIED_NUM,							/* Send SERVER_DENIED */
			NULL, NULL, NULL, NULL))) {					/* no parameters */
			if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
			return SERVER_DENIED_COMM;  // V
		}
		else // send   SERVER_DENIED  failed -> Communication failure...
			return sendRes;  //DON'T SET 'ERROR' EVENT!!!
		
	}
	else{
		// number is either 1 or 2 (Only the Server main thread will see the value 0). These set of values mean that 
		//	the current Client is either the first or second of two players, which means it is possible to continue the communication....
		
		//Send $$$ ^ SERVER_APPROVED ^ $$$ 
		
		if ((communicationResults)TRANSFER_SUCCEEDED == (sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_APPROVED_NUM,						/* Send SERVER_APPROVED */
			NULL, NULL, NULL, NULL))) {					/* no parameters */
			//Continue...>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			return COMMUNICATION_SUCCEEDED;
		}
		else {// send   SERVER_APPROVED  failed -> Communication failure...
			return sendRes;  //DON'T SET 'ERROR' EVENT!!!
		}
	}
}







static communicationResults initiateMainMenuProcedure(workingThreadPackage* p_params)
{
	message* p_receivedMessageFromClient = NULL;
	communicationResults commRes = 0, sendRes = 0;
	int currentlyConnectedClientsNumberDecrementationRes = 0;
	//Assert
	assert(NULL != p_params);


	while (TRUE) {
		//Send    $$$ ^ SERVER_MAIN_MENU ^ $$$
		if ((communicationResults)TRANSFER_SUCCEEDED != (sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_MAIN_MENU_NUM,						/* Send SERVER_MAIN_MENU */
			NULL, NULL, NULL, NULL))) {					/* no parameters */
			
			// send   SERVER_MAIN_MENU  may or may not fail -> returning the send result....
			return sendRes;  //DON'T SET 'ERROR' EVENT!!!
		}
		//Continue...>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>



		//Await Client responses...						
		commRes = mainMenuClientResponses(p_params);
		if (COMMUNICATION_SUCCEEDED != commRes) {
			return commRes;
		}


		
		//Received CLIENT_VERSUS



		//>>>>
		//3rd -Check the status of the "ERROR" & "EXIT" events to know if it is needed to end the current Worker thread
		switch (validateErrorExitEventsStatusInWorkerThread(p_params->p_h_errorEvent, p_params->p_h_exitEvent)) {
		case STATUS_SERVER_EXIT: //Close all resources of the current Worker thread e.g. player name & exit thread
			if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
			return COMMUNICATION_EXIT;
			
		case STATUS_SERVER_ERROR://Close all resources of the current Worker thread e.g. player name & exit thread
			if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
			return COMMUNICATION_EXIT;

		default: // KEEP_GOING (0)
			break; //Proceed.........>>>>
		}

		//printf("My Name:   %s\n", p_params->p_selfPlayerName); //DELETE'

		//>>>>
		//Synchronize between a First Player & a Second Player using Events objects, Create "GameSession.txt" file by one of 
		// them (Race to "GameSession.txt" resource Mutex) & transfer data, Begin & Conduct Game between the two, if they are indeed connect to each other
		switch(validateThereAreTwoPlayersInGameRoomAndSynchronizeThem(p_params)){
		case BACK_TO_MENU: continue; 
		case COMMUNICATION_FAILED: return COMMUNICATION_FAILED; //Indeed a fail!! not a disconnection from Client noticed by recv(.) and is defined as SERVER_DISCONNECT
		case COMMUNICATION_EXIT: return COMMUNICATION_EXIT;
		default: //COMMUNICATION_SUCCEEDED: 
			//Two players are synchronized - every Worker thread has the other User's name and is ready to send to the User SERVER_INVITE
			break;
		}
		//printf("My Name:   %s ,, Other Name:    %s\n", p_params->p_selfPlayerName, p_params->p_otherPlayerName); //'DELETE'
		//Set of the global variable of "Quit if one player left abruptly in the middle of the game"
		g_serverOpponentQuitBit = 0;

		//Proceed to GAME!
		switch(beginGame(p_params)){
		case BACK_TO_MENU:			    continue; break;
		case PLAYER_DISCONNECTED:	    continue; break; //For both  BACK_TO_MENU & PLAYER_DISCONNECTED  it is okay to resume connection with the Server. The User will choose the next step....
		case SERVER_DISCONNECTED:	 	return SERVER_DISCONNECTED; break;
		case COMMUNICATION_TIMEOUT:	 	return COMMUNICATION_TIMEOUT; break;
		case COMMUNICATION_FAILED:	 	return COMMUNICATION_FAILED; break;
		case COMMUNICATION_EXIT:		return COMMUNICATION_EXIT;
		case GRACEFUL_DISCONNECT:		return GRACEFUL_DISCONNECT;
		default:
			 return COMMUNICATION_SUCCEEDED;
		}


	
	
	} // while(True)
	
}

static communicationResults mainMenuClientResponses(workingThreadPackage* p_params)
{
	message* p_receivedMessageFromClient = NULL;
	transferResults tranRes = 0;
	int currentlyConnectedClientsNumberDecrementationRes = 0;
	//Assert
	assert(NULL != p_params);


	//Receive either _CLIENT_VERSUS_  or  _CLIENT_DISCONNECT_

	//Since Server's main menu demands the decision of the Client's User, then it is allowed to wait for a long time(10min)
	tranRes = receiveMessage(p_params->p_s_acceptSocket, &p_receivedMessageFromClient, LONG_SERVER_RESPONSE_WAITING_TIMEOUT);
	if (TRANSFER_SUCCEEDED == tranRes) //Validate the receive operation result...
		switch (p_receivedMessageFromClient->messageType) {
		case CLIENT_VERSUS_NUM:
			//Free the received message arranged in a 'message' struct
			freeTheMessage(p_receivedMessageFromClient);
			//Continue... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			return COMMUNICATION_SUCCEEDED;
			break;

		case CLIENT_DISCONNECT_NUM:
			//Free the received message arranged in a 'message' struct
			freeTheMessage(p_receivedMessageFromClient);
			 // No need to send back messages!!!
			return PLAYER_DISCONNECTED; // V
			break;

		default: //Received a wrong message /* no other message is expected from the Server at this point */
			printf("Recived an unexpected message. Exiting\n");
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			//Free the massage received
			freeTheMessage(p_receivedMessageFromClient);
			gracefulDisconnect(p_params->p_s_acceptSocket); //'CHECK TIMEOUT CASE'
			return COMMUNICATION_FAILED; break;
		}
	else {
		// The communication between the Server Worker thread & the Client Speaker thread failed during recv(.) function, 
		//   and transRes contains the reason which is also the thread's exit code IN THIS CASE
		if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		return (communicationResults)tranRes;
	}
}






static communicationResults validateThereAreTwoPlayersInGameRoomAndSynchronizeThem(workingThreadPackage* p_params)
{
	int currentlyConnectedClientsNumber = 0;// , firstPlayerBit = 0;//If the firstPlayerBit becomes 1, then this thread is the "First Player" of a couple
	transferResults sendRes = 0;
	//Assert
	assert(NULL != p_params);

	//Fetch the number of currently connected Clients
	currentlyConnectedClientsNumber = incrementDecrementNumOfCurrConnClientsAndHandleErrorEvent(p_params, 0/*fetch value*/);

		//Validate the return value of the resource value retrival operation...
	switch (currentlyConnectedClientsNumber) {
	case -1:
		return COMMUNICATION_FAILED; break; // not communication timeout

	case 1: // ONLY one player(User\Client) is connected to the Server...
		//Send   ^ SERVER_NO_OPPONENTS ^
		sendRes = (transferResults)sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_NO_OPPONENTS_NUM,					/* Send SERVER_NO_OPPONENTS */
			NULL, NULL, NULL, NULL);					/* no parameters */
		if (TRANSFER_SUCCEEDED == sendRes) {
			//Go back to MAIN MENU message send !!
			return BACK_TO_MENU;
		}
		else 
			return (communicationResults)sendRes; //Will send either COMMUNICATION_FAILED if sending operation failed(mem alloc.) or SERVER_DISCONNECTED if the Client abruptly disconnected
		break;

	case 2: // ONLY one player(User\Client) is connected to the Server...
		//BEGIN SYNCHRONIZING PROCEDURE...>>>>
		break;

	default: /*ignored*/
		printf("Default: not supposed to reach here.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		gracefulDisconnect(p_params->p_s_acceptSocket);//'CHECK' player name is free above?
		return COMMUNICATION_FAILED; 
	}
	
	//>>>>
	//Await that BOTH players responded to SERVER_MAIN_MENU with CLIENT_VERSUS so they are indeed ready to play!!
	switch (awaitBothPlayersByMarkingFirstClientThenSecondClient(p_params, 1/*transfer names*/, (int)SERVER_NO_OPPONENTS_NUM/*player 2 took too long to arrive*/)) {
	case COMMUNICATION_FAILED: return COMMUNICATION_FAILED;
	case BACK_TO_MENU: return BACK_TO_MENU;
	default: //COMMUNICATION_SUCCEEDED
		//Continue to "GameSession.txt" Creation & Inserting names >>>>>>>>>>
		break;
	}
	//printf("My Name:   %s\nOther Name:    %s", p_params->p_selfPlayerName, p_params->p_otherPlayerName); //'DELETE'

	//>>>>
	//4th -Check the status of the "ERROR" & "EXIT" events to know if it is needed to end the current Worker thread
	switch (validateErrorExitEventsStatusInWorkerThread(p_params->p_h_errorEvent, p_params->p_h_exitEvent)) {
	case STATUS_SERVER_EXIT: //Close all resources of the current Worker thread e.g. player name & exit thread
		if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket) || (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)))
			return COMMUNICATION_FAILED;
		//free(g_p_selfPlayerName); 'CHECK'
		return COMMUNICATION_EXIT;

	case STATUS_SERVER_ERROR://Close all resources of the current Worker thread e.g. player name & exit thread
		if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket) || (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)))
			return COMMUNICATION_FAILED;
		//free(g_p_selfPlayerName); 'CHECK'
		return COMMUNICATION_EXIT;

	default: // KEEP_GOING (0)
		break; //Proceed.........>>>>
	}

	//Two players exist in the Game Room & are synchronized
	return COMMUNICATION_SUCCEEDED;
}

//safe
static BOOL setPlayerEventToSignaled(HANDLE* p_h_playerEvent, workingThreadPackage* p_params, int playerId)
{
	//Asserts
	assert(NULL != p_h_playerEvent);
	assert(NULL != p_params);
	assert((1 == playerId) || (0 == playerId));
	printf("\nsignal back player no %d - 1 is first\n",playerId);
	//Set the "Player" Event to singled state
	if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*p_h_playerEvent)) {
		if (1 == playerId) printf("Error: Failed to set 'First Player' event to signaled state after 'Second Player' was late to arrive, with error code no. %ld - fatal error.\nExiting..\n\n", GetLastError());
		else  printf("Error: Failed to set 'Second Player' event to signaled state, with error code no. %ld - fatal error.\nExiting..\n\n", GetLastError());
		//Signal 'Error' Event....
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: SetEvent function failed
			printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
			//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
		}
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		//Notify Client Speaker thread, that his connection, a Worker thread, disconnects due to faulty
		gracefulDisconnect(p_params->p_s_acceptSocket);
		return STATUS_CODE_FAILURE;
	}

	return STATUS_CODE_SUCCESS;

}
static communicationResults awaitBothPlayersByMarkingFirstClientThenSecondClient(workingThreadPackage* p_params, int dataType, int clientAbsencyMessageType)
{
	communicationResults stepsRes = 0;
	//Assert
	assert(NULL != p_params);
	//assert(NULL != p_firstPlayerBitAddress);

	//Sample the First Player event..
	switch (WaitForSingleObject(*(p_params->p_h_firstPlayerEvent), SAMPLE)) {
	case WAIT_OBJECT_0: // This thread is the First arriving player & SECOND to access the "GameSession.txt" file..
		//THIS THREAD, meaning, This Server Worker thread associated with a connected Client, IS THE FIRST PLAYER  while there are two
		//	players connected to the Server...    Wait for a LONG time, until the second player agrees to play as well at any stage of the communication
		switch (WaitForSingleObject(*(p_params->p_h_secondPlayerEvent), LONG_SERVER_RESPONSE_WAITING_TIMEOUT)) {
		case WAIT_OBJECT_0:
			stepsRes = stepsStyleAccessingGameSessionFile(
				p_params,								/*thread inputs package*/
				dataType,								/*names transfer is the first operation, initial number is the second... dictated by data type*/
				0,										/*second accessing player*/
				1,										/*for the second accessing player, bit = D.C*/
				p_params->p_h_secondPlayerEvent,		/*"Stuck" event*/
				p_params->p_h_firstPlayerEvent);		/*"Release" event*/

						
			if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_firstPlayerEvent))) {
				printf("Error: Failed to set '1st Player' event to signaled state for following file accesses, with error code no. %ld.\nExiting..\n\n", GetLastError());
				stepsRes = COMMUNICATION_FAILED;
			}
			//SIGNAL the "1st Player" Event the SECOND time to free the AGAIN the other player....^	
			return stepsRes;  break; //Steps (Triple -'WRITE' 'READ&WRITE' 'READ') access completed Continue>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>	1st arriver	
		
		case WAIT_TIMEOUT: // Second Player, who is assumed to be connected, took too long time to decide to play (CLIENT_VERSUS  arrived after too long or didn't arrive)
			//Send   ^ SERVER_NO_OPPONENTS ^
			if (TRANSFER_SUCCEEDED == sendMessageServerSide(
				p_params->p_s_acceptSocket,					/* Client Socket */
				clientAbsencyMessageType,					/* was SERVER_NO_OPPONENTS. can also be SERVER_OPPONENT_QUIT */
				NULL, NULL, NULL, NULL)) {					/* no parameters */
				//Set the "First Player" Event for players synchronoization attempt
				if (STATUS_CODE_FAILURE == setPlayerEventToSignaled(p_params->p_h_firstPlayerEvent, p_params, 1)) return COMMUNICATION_FAILED;
				return BACK_TO_MENU; break;
			}
			else {// send   SERVER_NO_OPPONENTS  failed -> Communication failure...
				if (STATUS_CODE_FAILURE == setPlayerEventToSignaled(p_params->p_h_firstPlayerEvent, p_params, 1)) return COMMUNICATION_FAILED;
				return gracefulDisconnect(p_params->p_s_acceptSocket);  //will contain either the graceful case, the abrupt disconnection case and the failing case
			}
		
		default: // WaitForSingleObject(.) failed..
			printf("Error: Failed to wait on the Second Player 'door' - Event - using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
			//Signal 'Error' Event....
			if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: Mutex ownership\release failed
				printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
				//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
			}
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			return COMMUNICATION_FAILED; break; // not communication timeout
		}
		break;
	
	case WAIT_TIMEOUT: // This thread is the Second arriving Player, who will be the FIRST TO ACCESS GameSessio.txt and write to it !!!
		//THIS THREAD, meaning, This Server Worker thread associated with a connected Client, IS THE SECOND PLAYER  while there are two
		//	players connected to the Server...   
		
		stepsRes = stepsStyleAccessingGameSessionFile(
			p_params,								/*thread inputs package*/
			dataType,								/*names transfer is the first operation, initial number is the second... dictated by data type*/
			1,										/*first accessing player*/
			1,										/*for the first accessing player, writing goes first - bit=1*/
			p_params->p_h_firstPlayerEvent,			/*"Stuck" event*/
			p_params->p_h_secondPlayerEvent);		/*"Release" event*/

		//SIGNAL the "Second Player" Event object (which was initialized to non-signaled state) to release the Worker thread that arrived First..
		// after WRITING to the file^
		if(COMMUNICATION_SUCCEEDED == stepsRes) 
			stepsRes = stepsStyleAccessingGameSessionFile(
			p_params,								/*thread inputs package*/
				dataType,							/*names transfer is the first operation, initial number is the second... dictated by data type*/
			1,										/*first accessing player*/
			0,										/*for the first accessing player, reading is next - bit=0*/
			p_params->p_h_firstPlayerEvent,			/*"Stuck" event*/
			p_params->p_h_secondPlayerEvent);		/*"Release" event*/

		//SIGNAL the "Second Player" Event the SECOND time to free the AGAIN the other player....^
		//Also Set "1st Player" event for FOLLOWING "GameSessions.txt" synchronous accesses (becuase this event is reset(turns non-signaled) THREE TIMES,
		//	once by the first arriver, then by the second arriver, then by the second arriver, and the first arriver sets it ONLY TWICE)!!!!
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_firstPlayerEvent))) {
			printf("Error: Failed to set '1st Player' event to signaled state for following file accesses, with error code no. %ld.\nExiting..\n\n", GetLastError());
			stepsRes = COMMUNICATION_FAILED;
		}
		return stepsRes; break;  //Steps (Triple -'WRITE' 'READ&WRITE' 'READ') access completed Continue>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>	2nd arriver																	
	
	default:
		printf("Error: Failed to enter(wait) on the First Player 'door' - Event - using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
		//Signal 'Error' Event....
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: Mutex ownership\release failed
			printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
			//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
		}
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return COMMUNICATION_FAILED; break; // not communication timeout
	}
}

static communicationResults stepsStyleAccessingGameSessionFile(workingThreadPackage* p_params, int dataTypeBit, int firstPlayerBit, int writeBit, HANDLE* p_h_stuckEvent, HANDLE* p_h_releaseEvent)
{
	communicationResults res = COMMUNICATION_SUCCEEDED;
	//Assert
	assert(NULL != p_params);
	assert((1 == dataTypeBit) || (2 == dataTypeBit) || (3 == dataTypeBit));
	assert((1 == firstPlayerBit) || (0 == firstPlayerBit));
	assert((1 == writeBit) || (0 == writeBit));
	assert(NULL != p_h_stuckEvent);
	assert(NULL != p_h_releaseEvent);

	//This is the main protocol for accessing the "GameSession.txt" in order to allow a 'READ' operation of a 1st Player
	//	followed by a 'WRITE'->'READ' operations of a 2nd Player followed by a 'WRITE' operation of the 1st Player.
	//This will be executed in a "steps style". Under the assumption, one of the players is already STUCK on some event,
	//	the other player will enter this procedure, going through a "Release" event, operating in a manner dictated by the input Bits,
	//	then, the player will Set the "Release" event, and get itself "Stuck" in the "Stuck" event, until the other player, WHO IS NOW RELEASED
	//	from the event it was locked by,  releases the "Stuck" event......
	//NOTE: 1st Player & 2nd Player are re-ordered every accessing round anew...
	switch (WaitForSingleObject(*p_h_releaseEvent, SAMPLE)) {
	case WAIT_TIMEOUT:
		//printf("\n4\n\n");
		if (1 == firstPlayerBit) {
			if (1 == writeBit)
				switch (dataTypeBit) {
				case 1:if (STATUS_CODE_SUCCESS != firstToReachTheFileWriteWrapper(p_params, p_params->p_selfPlayerName, CREATE_ALWAYS))
					res = COMMUNICATION_FAILED; break;

				case 2:if (STATUS_CODE_SUCCESS != firstToReachTheFileWriteWrapper(p_params, p_params->p_selfInitialNumber, OPEN_ALWAYS))
					res = COMMUNICATION_FAILED; break;

				case 3:if (STATUS_CODE_SUCCESS != firstToReachTheFileWriteWrapper(p_params, p_params->p_selfCurrentGuess, OPEN_ALWAYS))
					res = COMMUNICATION_FAILED; break;

				default: /*ignored*/ 
					printf("Default: not supposed to reach here.\n");
					printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
					gracefulDisconnect(p_params->p_s_acceptSocket);//'CHECK' player name is free above?
					res = COMMUNICATION_FAILED; break;
				}
			else //(0==writeBit)
				switch (dataTypeBit) {
				case 1:if (NULL == (p_params->p_otherPlayerName = firstToReachTheFileReadWrapper(p_params)))
					res = COMMUNICATION_FAILED; break;

				case 2: if (NULL == (p_params->p_otherInitialNumber = firstToReachTheFileReadWrapper(p_params)))
					res = COMMUNICATION_FAILED; break;

				case 3: if (NULL == (p_params->p_otherCurrentGuess = firstToReachTheFileReadWrapper(p_params)))
					res = COMMUNICATION_FAILED; break;

				default: /*ignored*/ res = COMMUNICATION_FAILED; break;
				}
		}
		else {//(0 == firstPlayerBit)
			//printf("\n7\n\n");
			switch (dataTypeBit) {
			case 1:if (NULL == (p_params->p_otherPlayerName = secondToReachTheFileReadThenWriteWrapper(p_params, p_params->p_selfPlayerName)))
				res = COMMUNICATION_FAILED; break;

			case 2:if (NULL == (p_params->p_otherInitialNumber = secondToReachTheFileReadThenWriteWrapper(p_params, p_params->p_selfInitialNumber)))
				res = COMMUNICATION_FAILED; break;

			case 3:if (NULL == (p_params->p_otherCurrentGuess = secondToReachTheFileReadThenWriteWrapper(p_params, p_params->p_selfCurrentGuess)))
				res = COMMUNICATION_FAILED; break;

			default: /*ignored*/
				printf("Default: not supposed to reach here (players mmixed up).\n");
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				gracefulDisconnect(p_params->p_s_acceptSocket);//'CHECK' player name is free above?
				res = COMMUNICATION_FAILED; break;
			}
		}
		break;
	default:
		printf("Default: not supposed to reach here (Events mixed up).\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		gracefulDisconnect(p_params->p_s_acceptSocket);//'CHECK' player name is free above?
		res = COMMUNICATION_FAILED; break;
	}
	//Set "Release" event 
	if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*p_h_releaseEvent)) {  
		printf("Error: Failed to set event to signaled state for other player, with error code no. %ld.\nExiting..\n\n", GetLastError());
		res = COMMUNICATION_FAILED;
	}
	//Get stucked on the "Stuck" event
	switch (WaitForSingleObject(*p_h_stuckEvent, LONG_SERVER_RESPONSE_WAITING_TIMEOUT)) {
	case WAIT_OBJECT_0: break; //Proceed >>>>>>>
	case WAIT_TIMEOUT:
	default:
		if(1 == firstPlayerBit) printf("Error: Failed to enter(wait) on the 1st Player Event - using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
		else printf("Error: Failed to enter(wait) on the 2nd Player Event - using WaitForSingleObject(.), with error code no. %ld.", GetLastError());
		//Signal 'Error' Event....
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: Mutex ownership\release failed
			printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
			//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
		}
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		res = COMMUNICATION_FAILED; break; // not communication timeou
	}
	//>>>>>>>>>>>>> Return operation result
	return res;
}




















static communicationResults beginGame(workingThreadPackage* p_params)
{
	communicationResults initialNumberReceiveProcedureRes= 0;
	//Assert
	assert(NULL != p_params);

	//Begin the Client's initial number retrival procedure...
	initialNumberReceiveProcedureRes = receiveInitialPlayerNumber(p_params); 
	//Possible outputs: COMMUNICATION_FAILED, SERVER_DISCONNECTED, COMMUNICATION_TIMEOUT, GRACEFUL_DISCONNECT
	if (COMMUNICATION_SUCCEEDED != initialNumberReceiveProcedureRes) {
		//Erase contents from Game session file
		if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED;
		//Return the result of the initial player number retrival  or failure if erasing file failed
		return initialNumberReceiveProcedureRes;
	}


	//Exchange between the Worker threads the initial numbers. Do so by first synchronizing between the two players, 
	// "ERROR" Event, and then transfer the 4 digits numbers through the file...
	switch (syncPlayersAndTransferNumbers(p_params, 2/*initial player numbers*/)) {
	case COMMUNICATION_FAILED: fileTruncationForWhenGameEnds(p_params); return COMMUNICATION_FAILED; //Operation failed regardless of the file erasure operation outcome
	case SERVER_DISCONNECTED:
		//Erase contents from Game session file
		if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
			return SERVER_DISCONNECTED;
	case GRACEFUL_DISCONNECT:
		//Erase contents from Game session file
		if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
		return GRACEFUL_DISCONNECT;
	case COMMUNICATION_EXIT: 
		//Erase contents from Game session file
		if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
		return COMMUNICATION_EXIT;

	default://COMMUNICATION_SUCCEEDED
		break; //continue to asking for guess numbers from the Clients, then exchange the
			   // information at Server-side, then calculate game phases and send back results..
	}

	


	//>>>>
	while (TRUE)
	{
		switch (receivePlayersGuessesAndComputeResults(p_params)) {
		case COMMUNICATION_FAILED: 
			//Erase contents from Game session file
			if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
			return COMMUNICATION_FAILED;
		case SERVER_DISCONNECTED: 
			//Erase contents from Game session file
			if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
			return SERVER_DISCONNECTED;
		case GRACEFUL_DISCONNECT: 
			//Erase contents from Game session file
			if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
			return GRACEFUL_DISCONNECT;
		case COMMUNICATION_EXIT: 
			//Erase contents from Game session file
			if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
			return COMMUNICATION_EXIT;
			//Erase contents from Game session file
			if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
		case BACK_TO_MENU: 
			//Erase contents from Game session file
			if (STATUS_CODE_FAILURE == fileTruncationForWhenGameEnds(p_params)) return COMMUNICATION_FAILED; //Due to fatal error at erasure
			return BACK_TO_MENU; //Take note here, if you don't return to main menu
		default://COMMUNICATION_SUCCEEDED
			continue;

		}
	}

}

static communicationResults receiveInitialPlayerNumber(workingThreadPackage* p_params)
{
	int firstPlayerBit = 0;
	message* p_receivedMessageFromClient = NULL;
	transferResults sendRes = 0, recvRes = 0;
	//Assert
	assert(NULL != p_params);

	//Send   ^ SERVER_INVITE ^
	sendRes = sendMessageServerSide(																//$$$ STARTING FROM HERE  SERVER_OPPONENT_QUIT  SHOULD BE CHECKED $$$
		p_params->p_s_acceptSocket,					/* Client Socket */
		SERVER_INVITE_NUM,							/* Send SERVER_INVITE with the opponent's name as a single parameter */
		p_params->p_otherPlayerName,				/* pointer to the other name string parameter */
		NULL, NULL, NULL);							/* no parameters : 2,3,4 */

	if (TRANSFER_PREVENTED == sendRes) {
		//Some other operation within the Server malfunctioned the Server-Client connectivity - fatal error 
		gracefulDisconnect(p_params->p_s_acceptSocket); //Operaion failed regardless of gracefulDisconnect operation
		return COMMUNICATION_FAILED;
	}
	else if (TRANSFER_FAILED == sendRes) {
		//No need for a "Graceful disconnect" operation because the Client disconnected abruptly
		//But, starting from this point onward SERVER_OPPONENT_QUIT can be sent to the OTHER player, so it we set g_serverOpponentQuitBit to '1'
		//It is okay to do so without a Synchronous object, because there may be only 2 players (Worker threads) currently in game,
		// and because changing it leads to the immediate exit of the changer thread, which means that if one thread were to change it
		// then there would be no collision, but if two were to change it, then collision between them wouldn't matter cause they would both
		// seize connection, without being able to send to their Clients the SERVER_OPPONENT_QUIT message.  Evantually, the bit will be turned off when exiting to main menu 
		g_serverOpponentQuitBit = 1;
		return SERVER_DISCONNECTED;
	}

	if(g_serverOpponentQuitBit == 1)
		//TRANSFER_SUCCEEDED -> check if other player disconnected		 send ^ SERVER_OPPONENT_QUIT ^
		sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_OPPONENT_QUIT_NUM,					/* Send SERVER_OPPONENT_QUIT  */
			NULL, NULL, NULL, NULL);					/* no parameters */
	
	else
		//TRANSFER_SUCCEEDED ->		 send ^ SERVER_SETUP_REQUSET ^
		sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_SETUP_REQUSET_NUM,					/* Send SERVER_SETUP_REQUSET  */
			NULL, NULL, NULL, NULL);					/* no parameters */

	if (TRANSFER_PREVENTED == sendRes) {
		//Some other operation within the Server malfunctioned the Server-Client connectivity - fatal error 
		gracefulDisconnect(p_params->p_s_acceptSocket); //Operaion failed regardless of gracefulDisconnect operation
		g_serverOpponentQuitBit = 1;
		return COMMUNICATION_FAILED;
	}
	else if (TRANSFER_FAILED == sendRes) {
		//No need for a "Graceful disconnect" operation because the Client disconnected abruptly
		g_serverOpponentQuitBit = 1;
		return SERVER_DISCONNECTED;
	}
	//printf("\n\n9\n\n"); //'DELETE'


	//TRANSFER_SUCCEEDED ->		expect to receive  CLIENT_SETUP
	//Need to await the player's initial number, which means we to need to await a human's reponse - Long period 10min
	recvRes = receiveMessage(p_params->p_s_acceptSocket, &p_receivedMessageFromClient, LONG_SERVER_RESPONSE_WAITING_TIMEOUT);
	if (TRANSFER_SUCCEEDED == recvRes) //Validate the receive operation result...
		switch (p_receivedMessageFromClient->messageType) {
		case CLIENT_SETUP_NUM:
			if (COPY_OPPONENT_NAME_FAILED == copyPlayerNameOrFourDigitNumberString(p_receivedMessageFromClient, &p_params->p_selfInitialNumber)) {
				freeTheMessage(p_receivedMessageFromClient);
				if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: Mem alloc failed
					printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
					printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
					//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
				}
				//Notify Client Speaker thread, that his connection, a Worker thread, disconnects due to faulty
				gracefulDisconnect(p_params->p_s_acceptSocket);
				return COMMUNICATION_FAILED;
			}
			//Free the received message arranged in a 'message' struct
			freeTheMessage(p_receivedMessageFromClient);
			//Continue... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			return COMMUNICATION_SUCCEEDED;  break;

		default: //Received a wrong message /* no other message is expected from the Server at this point */
			g_serverOpponentQuitBit = 1;
			printf("Recived an unexpected message. Exiting\n");
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			freeTheMessage(p_receivedMessageFromClient);
			gracefulDisconnect(p_params->p_s_acceptSocket); //'CHECK TIMEOUT CASE'
			return COMMUNICATION_FAILED; break;
		}
	else {
		if ((communicationResults)recvRes == SERVER_DISCONNECTED)  g_serverOpponentQuitBit = 1;
		// The communication between the Server Worker thread & the Client Speaker thread failed during recv(.) function, 
		//   and transRes contains the reason which is also the thread's exit code IN THIS CASE
		if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		return (communicationResults)recvRes; //DON'T SET 'ERROR' EVENT!!!
	}
}





static communicationResults syncPlayersAndTransferNumbers(workingThreadPackage* p_params, int dataTypeBit)
{
	int firstPlayerBit = 0;
	message* p_receivedMessageFromClient = NULL;
	communicationResults initialNumberReceiveProcedureRes = 0;
	//Assert
	assert(NULL != p_params);

	switch (awaitBothPlayersByMarkingFirstClientThenSecondClient(p_params, dataTypeBit, (int)SERVER_OPPONENT_QUIT/*Opponent may quit during game*/)) {
	case COMMUNICATION_FAILED: return COMMUNICATION_FAILED;
	case SERVER_DISCONNECTED: return SERVER_DISCONNECTED;
	case GRACEFUL_DISCONNECT: return GRACEFUL_DISCONNECT;
	default://COMMUNICATION_SUCCESS
		break; // Continue to share data between Worker threads
	}

	//>>>>
	//5th - Check the status of the "ERROR" & "EXIT" events to know if it is needed to end the current Worker thread
	switch (validateErrorExitEventsStatusInWorkerThread(p_params->p_h_errorEvent, p_params->p_h_exitEvent)) {
	case STATUS_SERVER_EXIT: //Close all resources of the current Worker thread e.g. player name & exit thread
		if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		//free(g_p_selfPlayerName); 'CHECK'
		return COMMUNICATION_EXIT;

	case STATUS_SERVER_ERROR://Close all resources of the current Worker thread e.g. player name & exit thread
		if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		//free(g_p_selfPlayerName); 'CHECK'
		return COMMUNICATION_EXIT;

	default: // KEEP_GOING (0)
		break; //Proceed.........>>>>
	}

	return COMMUNICATION_SUCCEEDED;
	

}

static communicationResults receivePlayersGuessesAndComputeResults(workingThreadPackage* p_params)
{
	message* p_receivedMessageFromClient = NULL;
	transferResults sendRes = 0, recvRes = 0;
	//Assert
	assert(NULL != p_params);

	if(g_serverOpponentQuitBit == 1)
		//Send   ^ SERVER_OPPONENT_QUIT ^
		sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_OPPONENT_QUIT_NUM,				/* Send SERVER_OPPONENT_QUIT  */
			NULL, NULL, NULL, NULL);					/* no parameters  */
	
	
	else
		//Send   ^ SERVER_PLAYER_MOVE_REQUEST ^
		sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_PLAYER_MOVE_REQUEST_NUM,				/* Send SERVER_PLAYER_MOVE_REQUEST with the opponent's name as a single parameter */
			NULL,NULL, NULL, NULL);						/* no parameters  */

	if (TRANSFER_PREVENTED == sendRes) {
		//Some other operation within the Server malfunctioned the Server-Client connectivity - fatal error 
		gracefulDisconnect(p_params->p_s_acceptSocket); //Operaion failed regardless of gracefulDisconnect operation
		g_serverOpponentQuitBit = 1; //Exiting... Notify other Worker thread
		return COMMUNICATION_FAILED;
	}
	else if (TRANSFER_FAILED == sendRes) {
		//No need for a "Graceful disconnect" operation because the Client disconnected abruptly
		g_serverOpponentQuitBit = 1; //Exiting... Notify other Worker thread
		return SERVER_DISCONNECTED;
	}


	//printf("My number    %s ,, Other number     %s\n", p_params->p_selfInitialNumber, p_params->p_otherInitialNumber); 'DELETE'



	//TRANSFER_SUCCEEDED ->		expect to receive  *CLIENT_PLAYER_MOVE*
	//Need to await the player's guess number, which means we to need to await a human's reponse - Long period 10min
	recvRes = receiveMessage(p_params->p_s_acceptSocket, &p_receivedMessageFromClient, LONG_SERVER_RESPONSE_WAITING_TIMEOUT);
	if (TRANSFER_SUCCEEDED == recvRes) //Validate the receive operation result...
		switch (p_receivedMessageFromClient->messageType) {
		case CLIENT_PLAYER_MOVE_NUM:
			if (COPY_OPPONENT_NAME_FAILED == copyPlayerNameOrFourDigitNumberString(p_receivedMessageFromClient, &p_params->p_selfCurrentGuess)) {
				freeTheMessage(p_receivedMessageFromClient);
				if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: Mem alloc failed
					printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
					printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
					//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
				}
				//Notify Client Speaker thread, that his connection, a Worker thread, disconnects due to faulty
				gracefulDisconnect(p_params->p_s_acceptSocket);
				return COMMUNICATION_FAILED;
			}



			//Free the received message arranged in a 'message' struct
			freeTheMessage(p_receivedMessageFromClient);
			//Continue... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			  break;

		default: //Received a wrong message /* no other message is expected from the Server at this point */
			g_serverOpponentQuitBit = 1;
			printf("Recived an unexpected message. Exiting\n");
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			freeTheMessage(p_receivedMessageFromClient);
			gracefulDisconnect(p_params->p_s_acceptSocket); //'CHECK TIMEOUT CASE'
			return COMMUNICATION_FAILED; break;
		}
	else {
		if ((communicationResults)recvRes == SERVER_DISCONNECTED)  g_serverOpponentQuitBit = 1;
		// The communication between the Server Worker thread & the Client Speaker thread failed during recv(.) function, 
		//   and transRes contains the reason which is also the thread's exit code IN THIS CASE
		if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_acceptSocket)) return COMMUNICATION_FAILED;
		return (communicationResults)recvRes; //DON'T SET 'ERROR' EVENT!!!
	}

	//printf("\n\n11\n\n"); 'DELETE'
	//Exchange between the Worker threads the guess numbers. Do so by first synchronizing between the two players, 
	// "ERROR" Event, and then transfer the 4 digits numbers through the file...
	switch (syncPlayersAndTransferNumbers(p_params, 3/*guess numbers*/)) {
	case COMMUNICATION_FAILED: return COMMUNICATION_FAILED;
	case SERVER_DISCONNECTED: return SERVER_DISCONNECTED;
	case GRACEFUL_DISCONNECT: return GRACEFUL_DISCONNECT;
	case COMMUNICATION_EXIT: return COMMUNICATION_EXIT;
	default://COMMUNICATION_SUCCEEDED
		break; //continue to asking for guess numbers from the Clients, then exchange the
			   // information at Server-side, then calculate game phases and send back results..
	}

	//Calculate Round! Send results!
	switch (prepareResultsOfCurrentRoundAndSend(p_params)) {
	case COMMUNICATION_FAILED: return COMMUNICATION_FAILED;
	case SERVER_DISCONNECTED: return SERVER_DISCONNECTED;
	case GRACEFUL_DISCONNECT: return GRACEFUL_DISCONNECT;
	case BACK_TO_MENU: return BACK_TO_MENU;
	default: //COMMUNICATION_SUCCEEDED
		return COMMUNICATION_SUCCEEDED; break;
	}
	
}




static communicationResults prepareResultsOfCurrentRoundAndSend(workingThreadPackage* p_params)
{
	int firstPlayerBit = 0;
	message* p_receivedMessageFromClient = NULL;
	transferResults sendRes = 0, recvRes = 0;
	//"other" variables will contain the values of the results of the current thread comparisons (Current initial number, Other guesses)
	//"self" variables will contain the values of the results of the other thread comparisons (Other initial number, self guesses)
	SHORT selfGuessBulls = 0, otherGuessBulls = 0, selfGuessCows = 0, otherGuessCows = 0;
	TCHAR* p_winner = NULL, * sendBullsAndCowsBuffer = NULL, sendBullsCharacter = 'a', sendCowsCharacter = 'a';
	//Assert
	assert(NULL != p_params);

	//Conduct a single game phase
	playSingleGamePhase(
		p_params->p_otherInitialNumber,	/* Other Client(associated Worker thread) initial number */
		p_params->p_selfCurrentGuess,		/* This Client current guess number */
		&selfGuessBulls,					/* Bulls count of "self"(self's guess) side of the game */
		&selfGuessCows);					/* Cows count of "self"(self's guess) side of the game */
	playSingleGamePhase(
		p_params->p_selfInitialNumber,		/* Other Client(associated Worker thread) initial number */
		p_params->p_otherCurrentGuess,		/* This Client currnt guess number */
		&otherGuessBulls,					/* Bulls count of "other"(other's guess) side of the game */
		&otherGuessCows);					/* Cows count of "other"(other's guess) side of the game */



	
	

	//printf("\n\n12\n\n bu%c  co%c\n", sendBullsCharacter, sendCowsCharacter); 'DELETE'

	

	//Check results: 
	if ((selfGuessBulls == 4) && (otherGuessBulls == 4))
		return sendDraw(p_params); 				//Both players guessed their opponents' initial numbers correctly

	else if (selfGuessBulls == 4) {
		p_winner = p_params->p_selfPlayerName; 	//"Self" wins
		return sendWinner(p_params, p_winner);
	}
	else if (otherGuessBulls == 4) {
		p_winner = p_params->p_otherPlayerName;	//"Other" wins
		return sendWinner(p_params, p_winner);
	}
	else {										// None of the players guessed the other's initial number correctly
		//Allocate memory for buffer to hold the Bulls & Cows outcomes bytes
		if (NULL == (sendBullsAndCowsBuffer = (TCHAR*)calloc(sizeof(TCHAR), 4))) {
			printf("Error: Failed to allocate dynamic memory (Heap) for a buffer.\n");
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*(p_params->p_h_errorEvent))) {  //reason: Mem alloc failed
				printf("Error: Failed to set Exit/Error event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
				printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
				//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
			}
			return COMMUNICATION_FAILED;
		}
		//Prepare & Send a SERVER_GAME_RESULTS message with "Self" variables
		*sendBullsAndCowsBuffer = (TCHAR)('0' + selfGuessBulls);		//Implicit type case to int, then explicit typecast to TCHAR(which is char)
		*(sendBullsAndCowsBuffer + 2) = (TCHAR)('0' + selfGuessCows);	     //Implicit type case to int, then explicit typecast to TCHAR(which is char)
		//Since we used calloc, these addresses are kept in Heap and are followed by null character after each of them '\0'
		//so they can be related as null-terminated "strings" so we can send their addresses as type char*\TCHAR*

		if(g_serverOpponentQuitBit == 1)
			//Send   ^ SERVER_OPPONENT_QUIT ^
			sendRes = sendMessageServerSide(
				p_params->p_s_acceptSocket,					/* Client Socket */
				SERVER_OPPONENT_QUIT_NUM,				/* Send SERVER_OPPONENT_QUIT  */
				NULL, NULL, NULL, NULL);					/* no parameters  */

		else
			//Send    ^ SERVER_GAME_RESULTS ^
			sendRes = sendMessageServerSide(
				p_params->p_s_acceptSocket,							/* Client Socket */
				SERVER_GAME_RESULTS_NUM,							/* Send SERVER_GAME_RESULTS  */
				sendBullsAndCowsBuffer, sendBullsAndCowsBuffer + 2,	/* bulls & cows results as strings */
				p_params->p_otherPlayerName,						/* opponent username */
				p_params->p_otherCurrentGuess);						/* opponent guess number */

		//Validate sending result...
		if (TRANSFER_PREVENTED == sendRes) {
			//Some other operation within the Server malfunctioned the Server-Client connectivity - fatal error 
			free(sendBullsAndCowsBuffer);
			gracefulDisconnect(p_params->p_s_acceptSocket); //Operaion failed regardless of gracefulDisconnect operation
			//freeThePlayer(p_params);
			return COMMUNICATION_FAILED;
		}
		else if (TRANSFER_FAILED == sendRes) {
			//No need for a "Graceful disconnect" operation because the Client disconnected abruptly
			free(sendBullsAndCowsBuffer);
			//freeThePlayer(p_params);
			return SERVER_DISCONNECTED; // send SERVER_OPPONENT_QUIT and return BACK TO MENU
		}

		//Free bits string
		free(sendBullsAndCowsBuffer);
	}
	

	

	//Free the memory allcations of the "Guess"es numbers of both, the Other player & self, at the end of this Round 
	free(p_params->p_otherCurrentGuess); 
	p_params->p_otherCurrentGuess = NULL;
	free(p_params->p_selfCurrentGuess);
	p_params->p_selfCurrentGuess = NULL;

	//After analyzing the results, there appear to be no "winner" nor a "draw"
	// Cycle another one of guesses.....
	return COMMUNICATION_SUCCEEDED;
}




static communicationResults sendDraw(workingThreadPackage* p_params)
{
	transferResults sendRes = 0;
	communicationResults retVal = BACK_TO_MENU;
	//Assert
	assert(NULL != p_params);

	if (g_serverOpponentQuitBit == 1)
		//Send   ^ SERVER_OPPONENT_QUIT ^
		sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_OPPONENT_QUIT_NUM,				/* Send SERVER_OPPONENT_QUIT  */
			NULL, NULL, NULL, NULL);					/* no parameters  */
	
	else
		//Send    ^ SERVER_DRAW ^
		sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_DRAW_NUM,							/* Send SERVER_DRAW  */
			NULL, NULL, NULL, NULL);					/* no parameters  */

	if (TRANSFER_PREVENTED == sendRes) {
		//Some other operation within the Server malfunctioned the Server-Client connectivity - fatal error 
		gracefulDisconnect(p_params->p_s_acceptSocket); //Operaion failed regardless of gracefulDisconnect operation
		retVal = COMMUNICATION_FAILED;
	}
	else if (TRANSFER_FAILED == sendRes) {
		//No need for a "Graceful disconnect" operation because the Client disconnected abruptly
		g_serverOpponentQuitBit = 1;
		retVal = SERVER_DISCONNECTED;
	}
	//Free the memory allcations of ALL numbers of both, the Other player & self, and the opponents' name,
	//	and pointes the pointers to NULL, at the end of this Game... 
	//Other current guess
	free(p_params->p_otherCurrentGuess);
	p_params->p_otherCurrentGuess = NULL;
	//Self current guess
	free(p_params->p_selfCurrentGuess);
	p_params->p_selfCurrentGuess = NULL;
	//Other initial number
	free(p_params->p_otherInitialNumber);
	p_params->p_otherInitialNumber = NULL;
	//Self initial number
	free(p_params->p_selfInitialNumber);
	p_params->p_selfInitialNumber = NULL;
	//Other name
	free(p_params->p_otherPlayerName);
	p_params->p_otherPlayerName = NULL;

	//There is a tie!! We may return to Server's Main Menu
	return retVal; //Default is BACK_TO_MENU
}

static communicationResults sendWinner(workingThreadPackage* p_params, char* p_winner)
{
	transferResults sendRes = 0;
	communicationResults retVal = BACK_TO_MENU;
	//Assert
	assert(NULL != p_params);

	if (g_serverOpponentQuitBit == 1)
		//Send   ^ SERVER_OPPONENT_QUIT ^
		sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_OPPONENT_QUIT_NUM,				/* Send SERVER_OPPONENT_QUIT  */
			NULL, NULL, NULL, NULL);					/* no parameters  */

	else
		//Send    ^ SERVER_WIN ^
		sendRes = sendMessageServerSide(
			p_params->p_s_acceptSocket,					/* Client Socket */
			SERVER_WIN_NUM,								/* Send SERVER_WIN  */
			p_winner,									/* winner name  */
			p_params->p_otherInitialNumber,				/* opponenet inital number */
			NULL, NULL);								/* no parameters: 3,4  */

	if (TRANSFER_PREVENTED == sendRes) {
		//Some other operation within the Server malfunctioned the Server-Client connectivity - fatal error 
		gracefulDisconnect(p_params->p_s_acceptSocket); //Operaion failed regardless of gracefulDisconnect operation
		retVal = COMMUNICATION_FAILED;
	}
	else if (TRANSFER_FAILED == sendRes) {
		//No need for a "Graceful disconnect" operation because the Client disconnected abruptly
		g_serverOpponentQuitBit = 1;
		retVal = SERVER_DISCONNECTED;
	}

	//Free the memory allcations of ALL numbers of both, the Other player & self, and the opponents' name,
	//	and pointes the pointers to NULL, at the end of this Game... 
	//Other current guess
	free(p_params->p_otherCurrentGuess);
	p_params->p_otherCurrentGuess = NULL;
	//Self current guess
	free(p_params->p_selfCurrentGuess);
	p_params->p_selfCurrentGuess = NULL;
	//Other initial number
	free(p_params->p_otherInitialNumber);
	p_params->p_otherInitialNumber = NULL;
	//Self initial number
	free(p_params->p_selfInitialNumber);
	p_params->p_selfInitialNumber = NULL;
	//Other name
	free(p_params->p_otherPlayerName);
	p_params->p_otherPlayerName = NULL;

	//There is a winner!! We may return to Server's Main Menu
	return retVal; //Default is BACK_TO_MENU

}


static int copyPlayerNameOrFourDigitNumberString(message* p_receivedMessageFromClient, char** p_p_playerDataInMessage)
{
	int nameLength = 0;
	//Assert
	assert(NULL != p_receivedMessageFromClient);

	//Count the player's name length, either self or opponent
	nameLength = fetchStringLength(p_receivedMessageFromClient->p_parameters->p_parameter);

	//Allocate memory (Heap) for the player's name string
	if (NULL == (*p_p_playerDataInMessage = (char*)calloc(sizeof(char), nameLength + 1))) {
		printf("Error: Failed to allocate memory for the Opponent's name string.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return COPY_OPPONENT_NAME_FAILED;
	}
	//Copy the name to the newly allocated buffer
	concatenateStringToStringThatMayContainNullCharacters(
		*p_p_playerDataInMessage,									/* Destination Buffer */
		p_receivedMessageFromClient->p_parameters->p_parameter, /* Source Buffer */
		0,														/* Start position */
		nameLength);											/* Number of bytes to copy */

	//Copy succeeded...
	return COPY_OPPONENT_NAME_FAILED + 1;
}





static void playSingleGamePhase(TCHAR* p_opponentInitialDigits, TCHAR* p_selfPlayerGuessDigits, SHORT* p_bullsAddress, SHORT* p_cowsAddress)
{
	int i = 0, j = 0, bulls = 0, cows = 0;
	//Assert
	assert(NULL != p_opponentInitialDigits);
	assert(NULL != p_selfPlayerGuessDigits);
	assert(NULL != p_cowsAddress);
	assert(NULL != p_bullsAddress);



	//Play Game....!
	for (i; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			if ((*(p_selfPlayerGuessDigits + i) == *(p_opponentInitialDigits + j)) && (i == j))
				bulls++; /* Correct guess in the correct location - x2yz  m2np */
			else if ((*(p_selfPlayerGuessDigits + i) == *(p_opponentInitialDigits + j)) && (i != j))
				cows++; /* Correct guess in the different location - 2xyz  m2np */
		}
	}



	//Update Bulls & Cows results in the input addresses
	*p_cowsAddress = cows;
	*p_bullsAddress = bulls;
}
