/* ClientSideWorkingThreadRoutine.c
---------------------------------------------------------------------------------
	Module Description - This module contains functions meant for setting up the
		Client side communicating thread, also named as a Speaker Thread. It 
		contains the thread routine function & additional functions meant to 
		organize sections of the communication procedure. Also it contains 
		certain printing functions meant to represent to the Client's User the 
		results of the communication & possible games....
---------------------------------------------------------------------------------
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
#include "HardCodedData.h"
#include "ClientSideSpeakerThreadRoutine.h"


// Constants ------------------------------------------------------------
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;

static const int SINGLE_OBJECT = 1;

	//Duration constants
static const int SHORT_SERVER_RESPONSE_WAITING_TIMEOUT = 15000;	// 15 Seconds
static const int LONG_SERVER_RESPONSE_WAITING_TIMEOUT = 600000; // 600 Seconds = 10 Min
static const int KEEP_RECEIVE_TIMEOUT = -1;


static const int COPY_OPPONENT_NAME_FAILED = -1;

// Global variables ------------------------------------------------------------
//char* g_p_otherPlayerName = NULL;
char g_initialPlayerNumber[] = "null"; //Random string of length 5, length of game-numbers
char g_clientUserGuess[] = "null"; //4-digits inputs - Initialization - Random string of length 5, length of game-numbers


// Functions declerations ------------------------------------------------------

/// <summary>
/// Description - this function will send the CLIENT_REQUEST message and await the Server's response and act accordingly
/// If SERVER_APPROVED is received, the thread will continue to the Server main menu section, otherwise if SERVER_DENIED is
/// received, it means that this Client is a third player and it is rejected from communications back to "Connections menu"
/// where the User may decide to reconnect
/// </summary>
/// <param name="clientThreadPackage* p_params - thread's inputs (pointers to players name and Socket)"></param>
/// <returns>'communicationResults' code according to most of the codes possible</returns>
static communicationResults clientRequestSendingAndReceivingResponse(clientThreadPackage* p_params);
/// <summary>
/// Description - This is the Server's Main Menu loop function which always starts by expecting a SERVER_MAIN_MENU message, then it expects the
/// User (blocking) to enter a choice - if 1 is taken, then the Client will send the CLIENT_VERSUS in order to play a game, if - is chosen then the Client
/// will send teh CLIENT_DISCONNECT message, and begin the thread termination procedure. if the first is chosen, the function will also await the Server
/// resoponse, and if  SERVER_INVITE is received the function will continue to Gaming room.
/// </summary>
/// <param name="clientThreadPackage* p_params - thread's inputs (pointers to players name and Socket)"></param>
/// <returns>'communicationResults' code according to most of the codes possible</returns>
static communicationResults serverMainMenuLoop(clientThreadPackage* p_params);
/// <summary>
/// Description -  this function will send the CLIENT_VERSUS message and wait by the possible responses SERVER_INVITE if another player was to player with, or
/// SERVER_NO_OPPONENTS if this Client is the only connected client, but then, this function will return to the main menu loop and will expect SERVER_MAIN_MENU
/// message
/// </summary>
/// <param name="clientThreadPackage* p_params - thread's inputs (pointers to players name and Socket)"></param>
/// <returns>'communicationResults' code according to most of the codes possible </returns>
static communicationResults clientVersusSendingAndReceivingResponse(clientThreadPackage* p_params);
/// <summary>
/// Description - We entered a game! - This function begins awaiting the SERVER_SETUP_REQUEST message  that arrives after the SERVER_INVITE
/// message, then it proceeds tp asking for the initial 4 digits number picked by the User from STDin. and proceeds from there to send the CLIENT_SETUP
/// message with the initial number transferring it to the Server. From here the client enters the game loop to guess numbers
/// </summary>
/// <param name="clientThreadPackage* p_params - thread's inputs (pointers to players name and Socket)"></param>
/// <returns>'communicationResults' code according to most of the codes possible </returns>
static communicationResults gameRoomEntrance(clientThreadPackage* p_params);
/// <summary>
/// Description - This function cycles between expecting SERVER_PLAYER_MOVE_REQUEST and fetch the guess number from the User, and sending the User's guess
/// though a CLIENT_PLAYER_MOVE message followed by awaiting the SERVER_GAME_RESULTS\SERVER_WIN or SERVER_DRAW messages, while attempting to intercept the
/// SERVER_OPPONENT_QUIT message (broken functionality at the moment probably because of the Server). When one of the last three messages (of the 4) arrives,
/// the client Speaker thread return to Server's main menu to decide if the find another player to play against, or to quit... 
/// </summary>
/// <param name="clientThreadPackage* p_params - thread's inputs (pointers to players name and Socket)"></param>
/// <returns>'communicationResults' code according to most of the codes possible </returns>
static communicationResults gameLoop(clientThreadPackage* p_params);

/// <summary>
/// This function prints the results of every round in a "Bulls and Cows" game
/// </summary>
/// <param name="parameter* p_parameters - parameters may contain the number of bulls, cows, opponent name and opponent guess. all buffers with the struct(nested list)"></param>
static void printTheCurrentPhaseResultsToTheScreen(parameter* p_parameters);
/// <summary>
/// This function prints the results winning round in a "Bulls and Cows" game
/// </summary>
/// <param name="parameter* p_parameters - parameters will contain the opponent initial number and the winner name. all buffers with the struct(nested list)"></param>
static void printTheWinnerToTheScreen(parameter* p_parameters);


//typedef enum { TRANSFER_FAILED, TRANSFER_SUCCEEDED, TRANSFER_TIMEOUT, TRANSFER_DISCONNECTED } transferResults;
//typedef enum { COMMUNICATION_FAILED, COMMUNICATION_SUCCEEDED, COMMUNICATION_TIMEOUT, SERVER_DISCONNECTED, PLAYER_DISCONNECTED, SERVER_DENIED_COMM } communicationResults;

// Functions definitions -------------------------------------------------------

/*.... Exit codes: 
	Abrupt Server disconnection - 0 
	Correct operation - 1
	Timeout in recv(.) occured - 2
	Graceful Disconnection of Server - 3
	A fatal error occured (e.g. Mem alloc failure) - 4
	Disconnection of other player - 5 (Handled with the thread, because connection should remain and return to server's main menu)
	Server denied the Client's connection - 6

*/
communicationResults WINAPI clientSideSpeakerThreadRoutine(LPVOID lpParam)
{
	clientThreadPackage* p_params = NULL;
	message* p_receivedMessageFromServer = NULL;
	transferResults tranRes = 0;
	communicationResults mainMenuRes = 0, sendRes = 0, recvRes = 0, graceRes = 0;
	//Check whether lpParam is NULL - Input integrity validation
	if (NULL == lpParam) return EMPTY_THREAD_PARAMETERS;
	//Parameters input conversion from void pointer to section struct pointer by explicit type casting
	p_params = (clientThreadPackage*)lpParam;



	// Send ^CLIENT REQUEST^   &   Receive   _SERVER_DENIED_       OR        _SERVER_APPROVED_
	recvRes = clientRequestSendingAndReceivingResponse(p_params);
	if (COMMUNICATION_SUCCEEDED != recvRes) return recvRes;



	//Expect Main Menu....
	mainMenuRes = serverMainMenuLoop(p_params);
	//Validate Game procedure result.....
	switch (mainMenuRes) {
	case COMMUNICATION_SUCCEEDED: //Attemmpt Graceful Disconnect
		//Gracefull disconnect have 3 return values. the one that is not check, is where the Server did not respond to the graceful disconnect
		// but then we still choose to output the communication succeeded, because it did!
		graceRes = gracefulDisconnect(p_params->p_s_clientSocket);
		if (COMMUNICATION_FAILED == graceRes) return COMMUNICATION_FAILED;
		if (GRACEFUL_DISCONNECT == graceRes) return GRACEFUL_DISCONNECT;
		return COMMUNICATION_SUCCEEDED; break;

	case SERVER_DISCONNECTED: //Server already disconnected (was noticed at a failure of recv(.)\send(.) so Graceful Disconnect isn't needed
		return SERVER_DISCONNECTED; break;

	case COMMUNICATION_TIMEOUT: //Attemmpt Graceful Disconnect
		if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_clientSocket)) return COMMUNICATION_FAILED;
		return COMMUNICATION_TIMEOUT; break;

	case COMMUNICATION_FAILED: //Attemmpt Graceful Disconnect
		//FATAL ERROR OCCURED - Communication failed regardless of "Graceful Disconnect" operation
		gracefulDisconnect(p_params->p_s_clientSocket); 
		return COMMUNICATION_FAILED; break;

	default:/*ignored*/ return COMMUNICATION_FAILED;
	}
	

	
}




//......................................Static functions..........................................

static communicationResults clientRequestSendingAndReceivingResponse(clientThreadPackage* p_params)
{
	message* p_receivedMessageFromServer = NULL;
	transferResults tranRes = 0;
	communicationResults  sendRes = 0;
	//Assert
	assert(NULL != p_params);


	// Send   ^ CLIENT_REQUEST ^
	//Client connection established..... Send to the Server Worker set allocated to us(Client) the CLIENT_REQUEST message
	if ((communicationResults)TRANSFER_SUCCEEDED != (sendRes = sendMessageClientSide(
		p_params->p_s_clientSocket,					/* Client Socket */
		CLIENT_REQUEST_NUM,							/* Send CLIENT_REQUEST along with our(Player) name */
		p_params->p_playerName))) {					/* User's name (Our\Player) */

		// "Send" return with a different output code than COMMUNICATION_SUCCEEDED.. return that code
		return sendRes; //Graceful disconnect is included if needed (when a fatal error occurs)
	}


	//Continue >>>>



	// Receive _SERVER_APPROVED_  OR  _SERVER_DENIED_
	//Proceed to block this Client Speaker thread for the Server's response for 15 sec (set the receive timeout duration to 15 seconds)
	tranRes = receiveMessage(p_params->p_s_clientSocket, &p_receivedMessageFromServer, SHORT_SERVER_RESPONSE_WAITING_TIMEOUT);
	if (TRANSFER_SUCCEEDED == tranRes) //Validate the receive operation result...
		switch (p_receivedMessageFromServer->messageType) {
		case SERVER_APPROVED_NUM:
			freeTheMessage(p_receivedMessageFromServer);
			return COMMUNICATION_SUCCEEDED;//Continue...>>>>>>>>>>>>
			break;

		case SERVER_DENIED_NUM: //Contains also the case of "Third player rejection"  &&&&&&&&&&&&
			//Quit to Connection main menu
			freeTheMessage(p_receivedMessageFromServer);
			if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_clientSocket)) return COMMUNICATION_FAILED; //'CHECK TIMEOUT CASE'
			return SERVER_DENIED_COMM; break;

		default:/* no other message is expected from the Server at this point */
			return gracefulDisconnect(p_params->p_s_clientSocket);
			break;
		}
	else {
		// The communication between the Server Worker thread & the Client Speaker thread failed, 
		//   and transRes contains the reason which is also the thread's exit code IN THIS CASE
		if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_clientSocket)) return COMMUNICATION_FAILED;
		return (communicationResults)tranRes;
	}

	

}

static communicationResults serverMainMenuLoop(clientThreadPackage* p_params)
{
	communicationResults sendRes = 0, recvRes = 0, gameRoomResult = 0;
	transferResults tranRes = 0;
	message* p_receivedMessageFromServer = NULL;
	int clientUserResponse = 0; //Suitable for 1\2 answers & for 4-digits inputs
	//Assert
	assert(NULL != p_params);

	//MAIN MENU LOOP
	while (TRUE) {

		//>>>>
		// Receive  _SERVER_MAIN_MENU_
		//Proceed to block this Client Speaker thread for the Server's additional message, which is expected to be SERVER_MAIN_MENU,
		//	for 15 sec - meaning there is no need to alter the receive timeout 
		tranRes = receiveMessage(p_params->p_s_clientSocket, &p_receivedMessageFromServer, KEEP_RECEIVE_TIMEOUT);
		if (TRANSFER_SUCCEEDED == tranRes) //Validate the receive operation result...
			if (SERVER_MAIN_MENU_NUM == p_receivedMessageFromServer->messageType) {
				freeTheMessage(p_receivedMessageFromServer);
				//Continue >>>>>>>>>>>>>>>>>>>>>>>>
			}
			else {//Unexpected (WRONG) message arrived.... Exit with failure code
				printf("Recived an unexpected message. Exiting\n");
				printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
				freeTheMessage(p_receivedMessageFromServer);
				gracefulDisconnect(p_params->p_s_clientSocket); //Operaion failed regardless of gracefulDisconnect operation
				return COMMUNICATION_FAILED;
			}
		else {
			// The communication between the Server Worker thread & the Client Speaker thread failed, 
			//   and transRes contains the reason which is also the thread's exit code IN THIS CASE
			if (COMMUNICATION_FAILED == gracefulDisconnect(p_params->p_s_clientSocket)) return COMMUNICATION_FAILED;
			return (communicationResults)tranRes;
		}

		/*...........................................................*/
		/*...***...***...***	Input from User	   ***...***...***...*/
		/*...........................................................*/
		printf("Choose what to do next:\n1. Play against another client\n2. Quit\nType 1 or 2\n");
		//Proceed to block this Client Speaker thread to receive an input from STDin, which is expected to be SERVER_MAIN_MENU,
		if (SINGLE_OBJECT <= scanf_s("%d", &clientUserResponse)) {
			//User inserted 1
			if (1 == clientUserResponse) { // Play against a player
				//printf("RECEIVE vvv FAILED entered\n"); //'DELETE'


				// Send ^CLIENT VERSUS^   &   Receive   _SERVER_INVITE_  OR  _SERVER_NO_OPPONENTS_
				recvRes = clientVersusSendingAndReceivingResponse(p_params);
				if (COMMUNICATION_SUCCEEDED != recvRes) {
					if (BACK_TO_MENU == recvRes) continue;
					else return recvRes;
				}
			}



			//User inserted 2		-> (2 == clientUserResponse)   - according to instructions, we may assume correct inputs
			//							Quit - Initiate Shuting down connection
			else {	// Send  ^ CLIENT_DISCONNECT ^
				if ((communicationResults)TRANSFER_SUCCEEDED != (sendRes = sendMessageClientSide(
					p_params->p_s_clientSocket,					/* Client Socket */
					CLIENT_DISCONNECT_NUM,						/* Send CLIENT_DISCONNECT to QUIT */
					NULL))) {									/* no parameters */

					// send   CLIENT_SETUP  may or no may not failed 
					// "Send" return with a different output code than COMMUNICATION_SUCCEEDED.. return that code....
					return sendRes;
				}
				// Send succeeded... proceed to Graceful Disconnect						
				return COMMUNICATION_SUCCEEDED; // V


			} // User(client) Send finished for main menu ...


		}
		else {	// scanf_s failed
			printf("Error: Failed to collect a correct answer from STDin at Server's main menu.\n");
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			//gracefulDisconnect(p_params->p_s_clientSocket); //Operaion failed regardless of gracefulDisconnect operation
			return COMMUNICATION_FAILED;
		}

		//printf("\n\n%s\n\n", g_p_otherPlayerName);'DELETE'
		//printf("\n\n%s\n\n", p_params->p_otherPlayerName);'DELETE'

		//Reaching here means CLIENT_VERSUS was sent & SERVER_INVITE was accepted successfuly, which means a games is about to start!!!
		gameRoomResult = gameRoomEntrance(p_params);
			 //SWITCH  CASES: BACK_TO_MENU, PLAYER_DISCONNECTED, COMMUNICATION_FAILED, COMMUNICATION_TIMEOUT
			//if (STATUS_CODE_FAILURE == gracefulDisconnect(p_params->p_s_clientSocket)) return COMMUNICATION_FAILED; //'CHECK TIMEOUT CASE'
		//Validate Game procedure result.....
		switch (gameRoomResult) {
		case BACK_TO_MENU:		   continue; break; //  freeOtherPlayerName(); in all cases
		case PLAYER_DISCONNECTED: continue; break; //For both  BACK_TO_MENU & PLAYER_DISCONNECTED  it is okay to resume connection with the Server. The User will choose the next step....
		case SERVER_DISCONNECTED:  			return SERVER_DISCONNECTED; break;
		case COMMUNICATION_TIMEOUT:			return COMMUNICATION_TIMEOUT; break;
		case COMMUNICATION_FAILED: 			return COMMUNICATION_FAILED; break;
		default: 
			 return COMMUNICATION_SUCCEEDED;
		}

	}//while(TRUE)
}

static communicationResults clientVersusSendingAndReceivingResponse(clientThreadPackage * p_params)
{
	communicationResults sendRes = 0;
	transferResults tranRes = 0;
	message* p_receivedMessageFromServer = NULL;
	int clientUserResponse = 0; //Suitable for 1\2 answers & for 4-digits inputs
	//Assert
	assert(NULL != p_params);





	//Send  ^ CLIENT VERSUS ^
	if ((communicationResults)TRANSFER_SUCCEEDED != (sendRes = sendMessageClientSide(
		p_params->p_s_clientSocket,					/* Client Socket */
		CLIENT_VERSUS_NUM,							/* Send CLIENT_VERSUS to begin a match */
		NULL))) {									/* no parameters */

		// "Send" return with a different output code than COMMUNICATION_SUCCEEDED.. return that code
		return sendRes;
	}

	//Continue>>>>>
	//printf("expecting SERVER_INVITE\n"); 'DELETE'



	//Receive _SERVER_INVITE_  OR  _SERVER_NO_OPPONENTS_
	// Proceed to block the client on 'receive' and wait for either the second player to decide to play OR for the Server to respond there are no players. 
	// Either way set the receive timeout duration to 10 mins since we need to await a Second CONNECTED player, to suit the longer case of the two
	tranRes = receiveMessage(p_params->p_s_clientSocket, &p_receivedMessageFromServer, LONG_SERVER_RESPONSE_WAITING_TIMEOUT);
	if (TRANSFER_SUCCEEDED == tranRes) //Validate the receive operation result...
		switch (p_receivedMessageFromServer->messageType) {
		case SERVER_INVITE_NUM:
			
			freeTheMessage(p_receivedMessageFromServer); // FREES THE PLAYER NAME WE JUST RECEIVED
			//GAME IS ON!!!! Continue... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			return COMMUNICATION_SUCCEEDED;
			break;

		case SERVER_NO_OPPONENTS_NUM: //Contains also the case of "Third player rejection"
			//Back to Server's main menu - while(TRUE)
			printf("No opponents were found for conducting a game.\n");
			freeTheMessage(p_receivedMessageFromServer);
			return BACK_TO_MENU;  break;

		default:/* no other message is expected from the Server at this point */
			//printf("AAAA\n");
			printf("Recived an unexpected message no. %d. Exiting\n", p_receivedMessageFromServer->messageType);
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			return gracefulDisconnect(p_params->p_s_clientSocket);
			break;
		}
	//'CHECK' Also for the tranRes = TRANSFER_TIMEOUT case, printf  at SerComm.. 	printf("Failed connecting to server on %s:%hu.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\nType 1 or 2\n", p_ipAddressString, serverPortNumber);
	else return (communicationResults)tranRes; // Gracefull already occured. The communication between the Server Worker thread & the Client Speaker thread failed, and transRes contains the reason which is also the thread's exit code IN THIS CASE

}







static communicationResults gameRoomEntrance(clientThreadPackage* p_params)
{
	communicationResults sendRes = 0;
	transferResults tranRes = 0;
	message* p_receivedMessageFromServer = NULL;
	//Assert
	assert(NULL != p_params);

	printf("Game is on!\n");

	//Receive  _SERVER_SETUP_REQUSET_ 
	// Proceed to block this Client Speaker thread for the Server's additional message, which should be the one that asks
	// for an initial number of 4-different-digits. This message doesn't depend on any User's decision so the timeout should be 15 seconds. 
	tranRes = receiveMessage(p_params->p_s_clientSocket, &p_receivedMessageFromServer, SHORT_SERVER_RESPONSE_WAITING_TIMEOUT);
	if (TRANSFER_SUCCEEDED == tranRes) //Validate the receive operation result...
		if (SERVER_SETUP_REQUSET_NUM == p_receivedMessageFromServer->messageType) {
			freeTheMessage(p_receivedMessageFromServer);
			/*...........................................................*/
			/*...***...***...***	Input from User	   ***...***...***...*/
			/*...........................................................*/
			printf("Choose your 4 digits:\n");
			//Proceed to block this Client Speaker thread to receive an input from STDin,
			// which is expected to be the INITIAL 4-digits number for the game...
			if (SINGLE_OBJECT > scanf_s("%s", &g_initialPlayerNumber, EXIT_GUESS_LEN)) {
				// scanf_s failed
				printf("Error: Failed to collect a valid answer from STDin at Server's main menu.\n");
				printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
				//gracefulDisconnect(p_params->p_s_clientSocket); //Operaion failed regardless of gracefulDisconnect operation
				return COMMUNICATION_FAILED;
			}
			//Continue... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			
		}
		//Other player abruptly disconnected
		else if (SERVER_OPPONENT_QUIT_NUM == p_receivedMessageFromServer->messageType) {
			printf("\nOpponent quit.\n");
			freeTheMessage(p_receivedMessageFromServer);
			return PLAYER_DISCONNECTED;
		}

		else { //Received a wrong message
			printf("Recived an unexpected message no. %d. Exiting\n", p_receivedMessageFromServer->messageType);
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			freeTheMessage(p_receivedMessageFromServer);
			//gracefulDisconnect(p_params->p_s_clientSocket); //Operaion failed regardless of gracefulDisconnect operation
			return COMMUNICATION_FAILED;
		}
	//Receive operation (SERVER_SETUP_REQUSET) failed  - may arise COMMUNICATION_TIMEOUT COMMUNICATION_FAILED SERVER_DISCONNECTED
	//'CHECK' Also for the tranRes = TRANSFER_TIMEOUT case, printf  at SerComm.. 	printf("Failed connecting to server on %s:%hu.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\nType 1 or 2\n", p_ipAddressString, serverPortNumber);
	else return (communicationResults)tranRes; // Gracefull already occured. The communication between the Server Worker thread & the Client Speaker thread failed, and transRes contains the reason which is also the thread's exit code IN THIS CASE


	//printf("\n\n9\n");

	//Send  ^ CLIENT_SETUP ^
	if ((communicationResults)TRANSFER_SUCCEEDED != (sendRes = sendMessageClientSide(
		p_params->p_s_clientSocket,					/* Client Socket */
		CLIENT_SETUP_NUM,							/* Send CLIENT_SETUP to send self number */
		(char*)&g_initialPlayerNumber))) {			/* self player's number */

		// send   CLIENT_SETUP  may or no may not failed 
		// "Send" return with a different output code than COMMUNICATION_SUCCEEDED.. return that code....
		return sendRes;
	}
	

	//printf("\n\n10\n"); 'DELETE'
	//Enter Game loop.....
	switch(gameLoop(p_params)){
	case BACK_TO_MENU:					return BACK_TO_MENU; break;
	case PLAYER_DISCONNECTED:			return PLAYER_DISCONNECTED; break;
	case SERVER_DISCONNECTED:			return SERVER_DISCONNECTED; break;
	case COMMUNICATION_TIMEOUT:			return COMMUNICATION_TIMEOUT; break;
	case COMMUNICATION_FAILED:			return COMMUNICATION_FAILED; break;
	default: return COMMUNICATION_FAILED;
	}
}


static communicationResults gameLoop(clientThreadPackage* p_params)
{
	communicationResults sendRes = 0;
	transferResults tranRes = 0;
	message* p_receivedMessageFromServer = NULL;
	//Assert
	assert(NULL != p_params);

	//GAME LOOP
	while (TRUE) {
		//Receive _SERVER_PLAYER_MOVE_REQUEST_ 
		// The following message should arrive after the Server receives BOTH of the current phase's guesses numbers of the players
		// so the 'receive' timeout duration should be set to 10min to allow both players choose their initial numbers...
		tranRes = receiveMessage(p_params->p_s_clientSocket, &p_receivedMessageFromServer, LONG_SERVER_RESPONSE_WAITING_TIMEOUT);
		if (TRANSFER_SUCCEEDED == tranRes) //Validate the receive operation result...
			switch (p_receivedMessageFromServer->messageType) {
			case SERVER_PLAYER_MOVE_REQUEST_NUM:
				/*...........................................................*/
				/*...***...***...***	Input from User	   ***...***...***...*/
				/*...........................................................*/
				printf("Choose your guess:\n");
				//Proceed to block this Client Speaker thread to receive an input from STDin,
				// which is expected to be a GUESS of a 4-digits number for the a phase in the game...
				if (SINGLE_OBJECT > scanf_s("%s", &g_clientUserGuess, EXIT_GUESS_LEN)) {
					// scanf_s failed
					printf("Error: Failed to collect a correct answer from STDin at Server's main menu.\n");
					printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
					//gracefulDisconnect(p_params->p_s_clientSocket); //Operaion failed regardless of gracefulDisconnect operation
					return COMMUNICATION_FAILED;
				}
				freeTheMessage(p_receivedMessageFromServer);
				break; //Continue... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			
			case SERVER_OPPONENT_QUIT_NUM:
				printf("\nOpponent quit.\n");
				freeTheMessage(p_receivedMessageFromServer);
				return PLAYER_DISCONNECTED; break;
				//Return to MAIN MENU... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

			default:
				//Received a wrong message
				printf("Recived an unexpected message %d. Exiting\n", p_receivedMessageFromServer->messageType);
				printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
				freeTheMessage(p_receivedMessageFromServer);
				//gracefulDisconnect(p_params->p_s_clientSocket); //Operaion failed regardless of gracefulDisconnect operation
				return COMMUNICATION_FAILED;
				break;
			}
		//Receive operation (SERVER_SETUP_REQUSET) failed - may arise COMMUNICATION_TIMEOUT COMMUNICATION_FAILED SERVER_DISCONNECTED
		//'CHECK' Also for the tranRes = TRANSFER_TIMEOUT case, printf  at SerComm.. 	printf("Failed connecting to server on %s:%hu.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\nType 1 or 2\n", p_ipAddressString, serverPortNumber);
		else return (communicationResults)tranRes; // Gracefull already occured. The communication between the Server Worker thread & the Client Speaker thread failed, and transRes contains the reason which is also the thread's exit code IN THIS CASE

		//printf("\n\n11\n\n"); 'DELETE'

		// Send  ^ CLIENT_PLAYER_MOVE ^ 
		if ((communicationResults)TRANSFER_SUCCEEDED != (sendRes = sendMessageClientSide(
			p_params->p_s_clientSocket,					/* Client Socket */
			CLIENT_PLAYER_MOVE_NUM,						/* Send CLIENT_PLAYER_MOVE to send a 'guess' number of the opponent */
			(char*)&g_clientUserGuess))) {				/* self player's guess number of the opponent number */

			// send   CLIENT_PLAYER_MOVE  failed may or no may not failed 
			// "Send" return with a different output code than COMMUNICATION_SUCCEEDED.. return that code....
			return sendRes;
		}
		//Sending succeeded....





		// Receive   _SERVER_GAME_RESULTS_ 
		// The following message should arrive after the Server commputed the results of the guesses numbers of both players
		// so the 'receive' timeout duration should be set to 15sec because the operation doesn't demand further inputs from the Users (only computation of the Server)...
		tranRes = receiveMessage(p_params->p_s_clientSocket, &p_receivedMessageFromServer, SHORT_SERVER_RESPONSE_WAITING_TIMEOUT);
		if (TRANSFER_SUCCEEDED == tranRes) //Validate the receive operation result...
			switch(p_receivedMessageFromServer->messageType){
			case SERVER_GAME_RESULTS_NUM:
				printTheCurrentPhaseResultsToTheScreen(p_receivedMessageFromServer->p_parameters);
				freeTheMessage(p_receivedMessageFromServer);
				//LOOP FOR ANOTHER ROUND ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
				continue; break;

			case SERVER_WIN_NUM:
				printTheWinnerToTheScreen(p_receivedMessageFromServer->p_parameters);
				freeTheMessage(p_receivedMessageFromServer);
				return BACK_TO_MENU; break;
				//Return to MAIN MENU... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

			case SERVER_DRAW_NUM:
				printf("\nIt's a tie\n");
				freeTheMessage(p_receivedMessageFromServer);
				return BACK_TO_MENU; break;
				//Return to MAIN MENU... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

			case SERVER_OPPONENT_QUIT_NUM:
				printf("\nOpponent quit.\n");
				freeTheMessage(p_receivedMessageFromServer);
				return PLAYER_DISCONNECTED; break;
				//Return to MAIN MENU... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

			default:
				//Received a wrong message
				printf("Recived an unexpected message %d. Exiting\n", p_receivedMessageFromServer->messageType);
				printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
				freeTheMessage(p_receivedMessageFromServer);
				//gracefulDisconnect(p_params->p_s_clientSocket); //Operaion failed regardless of gracefulDisconnect operation
				return COMMUNICATION_FAILED;
			}
		//Receive operation (SERVER_SETUP_REQUSET) failed - may arise COMMUNICATION_TIMEOUT COMMUNICATION_FAILED SERVER_DISCONNECTED
		//'CHECK' Also for the tranRes = TRANSFER_TIMEOUT case, printf  at SerComm.. 	printf("Failed connecting to server on %s:%hu.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\nType 1 or 2\n", p_ipAddressString, serverPortNumber);
		else return (communicationResults)tranRes; // Gracefull already occured. The communication between the Server Worker thread & the Client Speaker thread failed, and transRes contains the reason which is also the thread's exit code IN THIS CASE
	
	} //while(TRUE)
}





static void printTheCurrentPhaseResultsToTheScreen(parameter* p_parameters)
{
	parameter* temp = NULL;
	//Assert
	assert(NULL != p_parameters);

	//.......Print _SERVER_GAME_RESULTS_ format message ........

	// Print the "Bulls" count of this round
	printf("\nBulls: %s\n", p_parameters->p_parameter);

	// Get the next parameter 
	p_parameters = p_parameters->p_nextParameter;
	// Print the "Cows" count of this round
	printf("Cows: %s\n", p_parameters->p_parameter);

	// Get the next parameter 
	p_parameters = p_parameters->p_nextParameter;
	// Print the "Opponent username" - player's name
	printf("%s played : ", p_parameters->p_parameter);

	// Get the next parameter 
	p_parameters = p_parameters->p_nextParameter;
	// Print the "Opponent move" - the CURRENT round opponent guess number
	printf("%s\n", p_parameters->p_parameter);
}

static void printTheWinnerToTheScreen(parameter* p_parameters)
{
	//Assert
	assert(NULL != p_parameters);

	//.......Print _SERVER_WIN_ format message.........

	// Print the "Winner" (name) of the game
	printf("\n%s won!\n", p_parameters->p_parameter);

	// Get the next parameter 
	p_parameters = p_parameters->p_nextParameter;
	// Print the "Opponent number" - the other player's INITIAL number
	printf("opponents number was %s\n", p_parameters->p_parameter);
}

