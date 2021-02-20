/* HardCodedData.h 
---------------------------------------------------------------------
	Module Description - Header module that is meant to contain
		Macros, constants, structures' definitions etc.
		Without functions declerations. There is no HardCodedData.c!
		This Module is relevant to both processes Client & Server.
---------------------------------------------------------------------
*/


#pragma once
#ifndef __HARD_CODED_DATA_H__
#define __HARD_CODED_DATA_H__


// Library includes -------------------------------------------------
#include <stdio.h>
#include <string.h>



// Constants
#define EMPTY_THREAD_PARAMETERS FALSE

#define SERVER_ADDRESS_STR "127.0.0.1"
#define MAX_ADDRESS_STR_LEN 16
#define MAX_PLAYER_NAME_LEN 20

#define STRINGS_ARE_EQUAL( Str1, Str2, len ) ( strncmp( (Str1), (Str2), len ) == 0 )
#define EXIT_GUESS_LEN 5

//.......BOTH constants
#define SET_EVENT_TO_SIGNALED_STATE_FAILED 0



//.......Server Constants
#define NUM_OF_WORKER_THREADS 3
#define CLIENT_AWAITS_CONNECTION 1
#define NO_CLIENT_PENDING_CONNECTION 0 //Select function Timeout

#define MUTEX_CONTROL_FAILURE -1
#define GAME_SESSION_PATH "GameSession.txt" //Relative Path to Server process files ONLY


	//"Exit" "Error" events status constants
#define KEEP_GOING 0
#define STATUS_SERVER_ERROR -1
#define STATUS_SERVER_EXIT -2



	//Messages
#define SERVER_MAIN_MENU_NUM 1
#define SERVER_MAIN_MENU "SERVER_MAIN_MENU" 

#define SERVER_APPROVED_NUM 2
#define SERVER_APPROVED "SERVER_APPROVED" 

#define SERVER_DENIED_NUM 3
#define SERVER_DENIED "SERVER_DENIED" 

#define SERVER_INVITE_NUM 4
#define SERVER_INVITE "SERVER_INVITE" 

#define SERVER_SETUP_REQUSET_NUM 5
#define SERVER_SETUP_REQUSET "SERVER_SETUP_REQUSET" 

#define SERVER_PLAYER_MOVE_REQUEST_NUM 6
#define SERVER_PLAYER_MOVE_REQUEST "SERVER_PLAYER_MOVE_REQUEST" 

#define SERVER_GAME_RESULTS_NUM 7
#define SERVER_GAME_RESULTS "SERVER_GAME_RESULTS" 

#define SERVER_WIN_NUM 8
#define SERVER_WIN "SERVER_WIN" 

#define SERVER_DRAW_NUM 9
#define SERVER_DRAW "SERVER_DRAW" 

#define SERVER_NO_OPPONENTS_NUM 10
#define SERVER_NO_OPPONENTS "SERVER_NO_OPPONENTS" 

#define SERVER_OPPONENT_QUIT_NUM 11
#define SERVER_OPPONENT_QUIT "SERVER_OPPONENT_QUIT" 




//.......Client constants

	//Messages
#define CLIENT_REQUEST_NUM 12
#define CLIENT_REQUEST "CLIENT_REQUEST" 

#define CLIENT_VERSUS_NUM 13
#define CLIENT_VERSUS "CLIENT_VERSUS" 

#define CLIENT_SETUP_NUM 14
#define CLIENT_SETUP "CLIENT_SETUP" 

#define CLIENT_PLAYER_MOVE_NUM 15
#define CLIENT_PLAYER_MOVE "CLIENT_PLAYER_MOVE"

#define CLIENT_DISCONNECT_NUM 16
#define CLIENT_DISCONNECT "CLIENT_DISCONNECT"


//Maybe need to dispose it!!!!!!!!!!!!!!!!!!!!!
typedef enum { IP_ADDRESS_IS_INVALID, LABEL_IS_INVALID, IP_ADDRESS_IS_VALID, LABEL_IS_VALID} ipAddressValidation;
typedef enum { CHARACTER_IS_DIGIT, CHARACTER_IS_UPPER_CASE, CHARACTER_IS_LOWER_CASE, CHARACTER_IS_HYPHEN, CHARACTER_IS_INVALID } characters;
typedef enum { STATUS_TIMEDOUT, STATUS_OWNED_SIGNALED, STATUS_ABONDAND, STATUS_FAILED } waitEnums;


typedef enum { TRANSFER_FAILED, TRANSFER_SUCCEEDED, TRANSFER_TIMEOUT, TRANSFER_DISCONNECTED, TRANSFER_PREVENTED } transferResults;



typedef enum {
	SERVER_DISCONNECTED,		//When recv(.) or send(.) fail because of reason that is NOT Timeout. & when recv(.) return #Bytes Sent == 0 is a Graceful disconnect
	COMMUNICATION_SUCCEEDED,	//ALL functions succeed - File accesses, Synchronous objects accesses, Socket's send & receive
	COMMUNICATION_TIMEOUT,		//recv(.) timeout
	GRACEFUL_DISCONNECT ,		//recv(.)'s bytesJustTransferred equals 0. Will occur after shudown with field SD_SENT
	COMMUNICATION_FAILED,		//Any fail possible which doesn't include recv(.) or send(.) failures... includes TRANSFER_PREVENTED
	PLAYER_DISCONNECTED,		//Player sent message "CLIENT_DISCONNECT" at "SERVER_MAIN_MENU" phase
	SERVER_DENIED_COMM,			//Server declined a Client Connection with "SERVER_DENIED". (main reason - 3rd player)
	BACK_TO_MENU,				//Mid way output - looping back to either Connection menu or Server main menu
	COMMUNICATION_EXIT			//"EXIT" or "ERROR" events were set - turned signaled
} communicationResults;



// Structures --------------------------------------------------------------------------------------------





	//parameter structure is used to store a parameter of a message meant to be transferred between Server & Client as a cell in a nested list. 
	// A nested-list is used rather than an, for example, an array, since for every message it will be unknown at the beginning how many parameters it should have.	
typedef struct _parameter {
	TCHAR* p_parameter;						// The value of the current parameter cell == LPTSTR
	struct _parameter* p_nextParameter;		// pointer to the next parameter struct
}parameter;


	//message structure is used to store a message's type described as a string(maybe a number) & nested list of the 'parameters' of the message
typedef struct _message {
	int messageType;				// message type as a serial number
	parameter* p_parameters;		// pointer to the parameter struct nested-list
}message;

	//messageString structure contains both the string of the message string in the formal phrasing & the string's size
typedef struct _messageString {
	DWORD messageLength;				// # of BYTES in the message string (not necessarily null-terminated characters array) - Buffer
	LPTSTR p_messageBuffer;				// pointer to the string containing a message type & its' parameters
}messageString;








//Thread input parameters struct (package) - This is a struct meant to combine all the inputs to a Working thread, which is a thread in the Server side
//											 meant to communicate with a Client process (Application), and will consist the communication socket with the Server
//											 and the pointers to all the needed synchronous objects.
typedef struct _workingThreadPackage {
	//Resource 1 - Number of current connected(-to-Server) Clients, will be modified identicaly to the number of existing working threads
	USHORT* p_currentNumOfConnectedClients;	// pointer to the number of existing working threads (resource)
	HANDLE* p_h_connectedClientsNumMutex;	// pointer to the number of existing working threads resource Mutex
	//Resource 2 - GameSession.txt file
	HANDLE* p_h_firstPlayerEvent;			// pointer to the Event in which a First player of two, has connected, its connection to Server was approved
											//		and this player's Working thread in the Server side created the GameSession.txt & wrote the player's name. 
											//		Also, it will be used to signal the Second player, during a game, that the First player Working thread has
											//		finished writing a "guess" to the file, at every phase of the game.
	HANDLE* p_h_secondPlayerEvent;			// pointer to the Event in which a Second player of two, has connected, its connection to Server was approved
											//		and this player's Working thread in the Server side wrote the player's name. Also, it will be used to 
											//		signal the First player, during a game, that the Second player Working thread has finished writing a "guess"
											//		to the file, at every phase of the game.
	//NOT a resource 
	HANDLE* p_h_exitEvent;					// pointer to the Event that will signal if "Exit" was entered in Server's STDin 										
	HANDLE* p_h_errorEvent;					// pointer to the Event that will signal if any fatal error has occured when, for example, Heap memory allocation
											//		failed, sync object accessing failed
	//Most importantly
	SOCKET* p_s_acceptSocket;				// pointer to the Server socket "accept" has outputted after accepting a Client's connection
	char* p_selfPlayerName;					// pointer to the a string repersenting the name of the current Client User
	char* p_otherPlayerName;				// pointer to the a string repersenting the name of the opponent Client User
	char* p_selfInitialNumber;				// pointer to the string represening the initial number of the current Client User
	char* p_otherInitialNumber;				// pointer to the string represening the initial number of the opponent Client User
	char* p_selfCurrentGuess;				// pointer to the string represening the current guess number of the current Client User
	char* p_otherCurrentGuess;				// pointer to the string represening the current guess number of the opponent Client User

}workingThreadPackage;



typedef struct _clientThreadPackage {
	char* p_playerName;						// pointer to the player's name, represented by the "Client" process User
	char* p_otherPlayerName;				// pointer to the other player's name
	SOCKET* p_s_clientSocket;				// pointer to the Server socket "accept" has outputted after accepting a Client's connection

}clientThreadPackage;

#endif //__HARD_CODED_DATA_H__