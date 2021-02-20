/* MessagesTransferringTools.c
---------------------------------------------------------------------------------
	Module Description - This module contains functions meant for constructing
		buffer with data needed to be sent (Messages needed to be sent) & 
		functions meant for retriving a recived messages information - message
		type and message parameters...
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
#include "MessagesTransferringTools.h"



// Constants
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;

static const int SINGLE_OBJECT = 1;





// Functions declerations ------------------------------------------------------

/// <summary>
/// Description - This function assembles the message data into a buffer and inserts it into a "messageType" data struct (no parameters)
/// </summary>
/// <param name="char* p_messageTypeString - pointer to message type string"></param>
/// <returns>pointer to a 'messageString' containing a pointer to the buffer containing the message type string and its size</returns>
static messageString* constructMessageStringWithNoParameters(char* p_messageTypeString);

/// <summary>
/// Description - same as constructMessageStringWithNoParameters(.) but also updates the 'parameter' nested-list struct with Brute Force strings
/// copying according to the input parameters buffers
/// </summary>
/// <param name="char* p_messageTypeString - string buffer pointer"></param>
/// <param name="char* p_paramOne - param 1"></param>
/// <param name="char* p_paramTwo - param 2"></param>
/// <param name="char* p_paramThree - param 3"></param>
/// <param name="char* p_paramFour - param 4"></param>
/// <returns>pointer to a 'messageString' containing a pointers to the buffer containing the message type and to the nested list buffers and its size</returns>
static messageString* constructMessageStringWithParameters(char* p_messageTypeString,
	char* p_paramOne, char* p_paramTwo, char* p_paramThree, char* p_paramFour);


//Start: Set '1' of functions contructing a "messageString" struct updated with a pointer to a buffer that contains data to transmit to some Client from the Server
//		 Some receives no parameters and use constructMessageStringWithNoParameters(.) for message string construction, while other have multiple parameters
//		 and use constructMessageStringWithParameters(.) for message string construction. The out put is a pointer to that "messageString" that contains the buffer and its size
static messageString* constructServerMainMenuMessageString();
static messageString* constructServerApprovedMessageString();
static messageString* constructServerDeniedMessageString(/*char* p_paramOne Denied reason*/);
static messageString* constructServerInviteMessageString(char* p_paramOne /*Other player name*/);
static messageString* constructServerSetupRequestMessageString();
static messageString* constructServerPlayerMoveRequestMessageString();
static messageString* constructServerGameResultsMessageString(char* p_paramOne/*#Bulls*/, char* p_paramTwo/*Cows*/, char* p_paramThree/*other player name*/, char* p_paramFour/*other guess*/);
static messageString* constructServerWinMessageString(char* p_paramOne/*winner name*/, char* p_paramTwo/*Other player's initial number*/);
static messageString* constructServerDrawMessageString();
static messageString* constructServerNoOpponentsMessageString();
static messageString* constructServerOpponentQuitMessageString();
//End set '1'

//Start: Set '2' of functions with similiar functionality as set '1', but the data will be transmitted to the Server from some Client 
static messageString* constructClientRequestMessageString(char* p_paramOne /*User name*/);
static messageString* constructClientVersusMessageString();
static messageString* constructClientSetupMessageString(char* p_paramOne /*Initial self number*/);
static messageString* constructClientPlayerMoveMessageString(char* p_paramOne /*self guess of opponent number*/);
static messageString* constructClientDisconnectMessageString();
//End set '2'



/// <summary>
/// Desciption - The function allocates memory to a 'messageString' object
/// </summary>
/// <returns>a pointer to that allocated struct, or NULL if failed</returns>
static message* allocateMemoryForMessageStruct();

/// <summary>
/// Description - This function allocates memory to a 'parameter' object, and prepares hard copy of the paramter inputted
/// by using int concatenateStringToStringThatMayContainNullCharacters(.)
/// </summary>
/// <param name="char* parameterStringLength - length of parameter buffer"></param>
/// <param name="char* p_bufferInitialCopyPosition - buffer may contain for than one parameter, this is the pointer to the starting copying position of the source buffer (from which we copy)"></param>
/// <returns>a pointer to that allocated struct, or NULL if failed</returns>
static parameter* allocateMemoryForParameterStruct(int parameterStringLength, char* p_bufferInitialCopyPosition);

/// <summary>
/// Description - This function assist in finding a certain byte character, assuming it exists, or if the string has a Carriage Return in it... (function to analize received messages)
/// (Since it was decided for a sending message to appear as "<data>\r\n\0" then the last character of the last parameter in a string will be folled by CR 
/// </summary>
/// <param name="char* p_receivedBuffer - received buffer from recv(.) function"></param>
/// <param name="int scanStartingPosition - starting scanning position in the received buffer"></param>
/// <param name="char mark - character indicator for end-of-scan"></param>
/// <returns>length of scanned section in bytes</returns>
static int findPositionOffsetOfGivenCharacterInBuffer(char* p_receivedBuffer, int scanStartingPosition, char mark);

/// <summary>
/// Description - This function already KNOW what is the the type of the received message, and it receives the received message buffer and
/// extracts, one-by-one, the parameters of this message and inserts them into a newly allocated buffer and inserts that into a newly allocated parameter
/// struct. It return status code regarding the successfullness of the operation
/// </summary>
/// <param name="char* p_receivedBuffer - received message buffer pointer"></param>
/// <param name="message* p_receivedMessageInfo - message struct pointer to insert the new constructed message parameter nested list into it"></param>
/// <param name="int messageTypeSerialNumber - message type identifier number"></param>
/// <param name="int bufferCurrentPosition - current position in the received message buffer where the message type string exectly ended  - ':' character"></param>
/// <returns>True if succeeded. False otherwise</returns>
static BOOL extractMessageParameters(char* p_receivedBuffer, message* p_receivedMessageInfo, int messageTypeSerialNumber, int bufferCurrentPosition);

/// <summary>
/// Description - This function updates a newly allocated inputted 'message' object with the message's fields - types and parameters, using
/// extractMessageParameters function, after classifying the message's type, to understand if it has parameters at all
/// </summary>
/// <param name="char* p_receivedBuffer - pointer to the received buffer"></param>
/// <param name="message* p_receivedMessageInfo - pointer to an already allocated 'message' struct"></param>
/// <returns>True if succeeded. False otherwise</returns>
static BOOL extractMessageInfo(char* p_receivedBuffer, message* p_receivedMessageInfo);






// Functions definitions -------------------------------------------------------

messageString* constructMessageForSendingServer(int messageType, char* p_paramOne, char* p_paramTwo, char* p_paramThree, char* p_paramFour)
{
	//Input integrity validation
	if ((SERVER_OPPONENT_QUIT_NUM < messageType) || (SERVER_MAIN_MENU_NUM > messageType)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return NULL;
	}
	//Construct message according to the message type's number....
	switch (messageType) {
		//Server messages (for sending)
	case SERVER_MAIN_MENU_NUM:
		return constructServerMainMenuMessageString(); break; //if(NULL == constructServerMainMenuMessageString())
	case SERVER_APPROVED_NUM:
		return constructServerApprovedMessageString(); break;
	case SERVER_DENIED_NUM:
		return constructServerDeniedMessageString(p_paramOne/*temp need to check in forum*/); break; //p
	case SERVER_INVITE_NUM:
		return constructServerInviteMessageString(p_paramOne); break; //p
	case SERVER_SETUP_REQUSET_NUM:
		return constructServerSetupRequestMessageString(); break;
	case SERVER_PLAYER_MOVE_REQUEST_NUM:
		return constructServerPlayerMoveRequestMessageString(); break;
	case SERVER_GAME_RESULTS_NUM:
		return constructServerGameResultsMessageString(p_paramOne, p_paramTwo, p_paramThree, p_paramFour); break; //p
	case SERVER_WIN_NUM:
		return constructServerWinMessageString(p_paramOne, p_paramTwo); break; //p
	case SERVER_DRAW_NUM:
		return constructServerDrawMessageString(); break;
	case SERVER_NO_OPPONENTS_NUM:
		return constructServerNoOpponentsMessageString(); break;
	case SERVER_OPPONENT_QUIT_NUM:
		return constructServerOpponentQuitMessageString(); break;



	default: return NULL; break;
	}
}
messageString* constructMessageForSendingClient(int messageType, char* p_paramOne)
{
	//Input integrity validation
	if ((CLIENT_DISCONNECT_NUM < messageType) || (CLIENT_REQUEST_NUM > messageType)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return NULL;
	}
	//Construct message according to the message type's number....
	switch (messageType) {
		//Client messages (for sending)
	case CLIENT_REQUEST_NUM:
		return constructClientRequestMessageString(p_paramOne); break; //p
	case CLIENT_VERSUS_NUM:
		return constructClientVersusMessageString(); break;
	case CLIENT_SETUP_NUM:
		return constructClientSetupMessageString(p_paramOne); break; //p
	case CLIENT_PLAYER_MOVE_NUM:
		return constructClientPlayerMoveMessageString(p_paramOne); break; //p
	case CLIENT_DISCONNECT_NUM:
		return constructClientDisconnectMessageString(); break;



	default: return NULL; break;
	}
}


message* translateReceivedMessageToMessageStruct(char* p_receivedBuffer)
{
	//char* p_receivedBuffer = NULL;
	message* p_receivedMessageInfo = NULL;
	//Input integrity validation
	if (NULL == p_receivedBuffer) {
		printf("Error: Bad inputs to function: %s\n", __func__); return NULL;
	}

	if (NULL == (p_receivedMessageInfo = allocateMemoryForMessageStruct())) {
		printf("Error: Failed to create a 'message' struct to contain the received message information.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}

	//Extract the message info from the received buffer. Expect, either the message contains more than
	// one parameter, so at the end of the message type a ':' character is placed, and there would be no '\0' beforehand,
	// so we can simply search for a ':'.  OR  the message doesn't contain parameters, so the after the message type a 
	// Carriage Return & Line Feed will be found...
	if (STATUS_CODE_FAILURE == extractMessageInfo(p_receivedBuffer, p_receivedMessageInfo)) {
		printf("Error: Failed to extract the input message information from the received buffer.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		freeTheMessage(p_receivedMessageInfo);
		//There is no need for the received buffer after analyzing it..
		free(p_receivedBuffer);
		return NULL;
	}
	//There is no need for the received buffer after analyzing it.. (all parameters are placed in separate strings)
	free(p_receivedBuffer); //maybe not 'CHECK'

	//Return the message struct info: type & parameters
	return p_receivedMessageInfo;
}






//......................................Static functions..........................................



//0oo0ooo0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o Functions mainly to construct a message to be sent
int fetchStringLength(char* p_stringBuffer)
{
	int currentLength = 0;
	char* p_currentPointerPosition = NULL;
	//Assert
	assert(NULL != p_stringBuffer);

	p_currentPointerPosition = p_stringBuffer;

	//Search length
	while ('\0' != *(p_stringBuffer + currentLength))
		currentLength++;

	//When p_stringBuffer in currentLength position is the 'zero' character, then the end of the message was reached 
	// and currentLength has become the TRUE length of the message in the buffer!
	return currentLength;
}
int concatenateStringToStringThatMayContainNullCharacters(char* p_destBuffer, char* p_sourceBuffer, int currentPositionOfDestBuffer, int numberOfBytesToWrite)
{
	int i = 0;
	//Asserts
	assert(NULL != p_destBuffer);
	assert(NULL != p_sourceBuffer);
	assert(0 <= currentPositionOfDestBuffer);
	assert(0 < numberOfBytesToWrite);

	//Begin strings copy\concatenation
	while (numberOfBytesToWrite > 0) {
		*(p_destBuffer + currentPositionOfDestBuffer) = *(p_sourceBuffer + i); //Pointers arithmetics
		i++;								//Advance the source buffer position 
		numberOfBytesToWrite--;				//Decrement the number bytes remaining to write to the destination buffer
		currentPositionOfDestBuffer++;		//Advance the destination buffer current position
	}
	
	//Return the last position index the concatenation had reached....
	return currentPositionOfDestBuffer;
}




static messageString* constructMessageStringWithNoParameters(char* p_messageTypeString)
{
	messageString* p_messageString = NULL;
	int messageTypeStringLength = 0;
	//Allocate Heap memory for a "message" struct
	if (NULL == (p_messageString = (messageString*)calloc(sizeof(messageString), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate dynamic memory (Heap) for a 'messageString' struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}

	//Calculate message type string length
	messageTypeStringLength = fetchStringLength(p_messageTypeString);

	//Allocate Heap memory for a "message"'s type string
	if (NULL == (p_messageString->p_messageBuffer = (TCHAR*)calloc(sizeof(TCHAR), messageTypeStringLength+3))) { // 3== CR + LF + '\0'
		printf("Error: Failed to allocate dynamic memory (Heap) string.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		free(p_messageString);
		return NULL;
	}

	//Insert message type string into messageString struct - NOTE: HERE string function e.g. sprintf_s would work because there are no '\0' data involved.
	if (EOF == sprintf_s(p_messageString->p_messageBuffer, messageTypeStringLength + 3, "%s\r\n", p_messageTypeString)) {
		printf("Error: Failed to copy the message type into a 'message' struct\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		free(p_messageString->p_messageBuffer);
		free(p_messageString);
		return NULL;
	}

	

	//Because this type of messages, for either Server or Clients, have NO parameters, 
	//	then the length of the message type string is the length of the entire message!!!
	p_messageString->messageLength = messageTypeStringLength;

	//Return the allocated & updated "message" struct..
	return p_messageString;
}

static messageString* constructMessageStringWithParameters(char* p_messageTypeString,
	char* p_paramOne, char* p_paramTwo, char* p_paramThree, char* p_paramFour) 
{
	messageString* p_messageString = NULL;
	int messageTypeStringLength = 0, paramOneLength = 0, paramTwoLength = 0, paramThreeLength = 0, paramFourLength=0, totalLen = 0, currentBufferPosition= 0;

	//Allocate Heap memory for a "message" struct
	if (NULL == (p_messageString = (messageString*)calloc(sizeof(messageString), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate dynamic memory (Heap) for a 'messageString' struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}

	//Calculate message type string length & The length of all existing parameters strings & Total message string length
	messageTypeStringLength = fetchStringLength(p_messageTypeString);
	paramOneLength = fetchStringLength(p_paramOne); //At least the first parameter exist, because if it doesn't then constructMessageStringWithNoParameters(.) is used instead
	totalLen =  messageTypeStringLength + 1/*':'*/ + paramOneLength;
	//If the Second paramter isn't NULL then add its length with ';'
	if (NULL != p_paramTwo) {
		paramTwoLength = fetchStringLength(p_paramTwo);
		totalLen = totalLen + 1/*';'*/ + paramTwoLength;
	}
	//If the Third paramter isn't NULL then add its length with ';'
	if (NULL != p_paramThree) {
		paramThreeLength = fetchStringLength(p_paramThree);
		totalLen = totalLen + 1/*';'*/ + paramThreeLength;
	}
	//If the Fourth paramter isn't NULL then add its length with ';'
	if (NULL != p_paramFour) {
		paramFourLength = fetchStringLength(p_paramFour);
		totalLen = totalLen + 1/*';'*/ + paramFourLength;
	}
	totalLen = totalLen + 3/*'\r\n\0'*/;
	
	
	//Allocate Heap memory for a "message"'s type string
	if (NULL == (p_messageString->p_messageBuffer = (TCHAR*)calloc(sizeof(TCHAR), totalLen))) {
		printf("Error: Failed to allocate dynamic memory (Heap) string.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		free(p_messageString);
		return NULL;
	}


	/* ------------------------------------------------------------------------------------------------- */
	/*		.....	Overall - "<message_type>:<param1>;<param2>;<param3>\n"		.....				     */
	/* ------------------------------------------------------------------------------------------------- */

	//Insert message type string into messageString struct - NOTE: HERE string function e.g. sprintf_s would work because there are no '\0' data involved.
	if (EOF == (currentBufferPosition = sprintf_s(p_messageString->p_messageBuffer, messageTypeStringLength + 1, "%s", p_messageTypeString))) {
		printf("Error: Failed to copy the message type into a 'messageString' struct's string\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		free(p_messageString->p_messageBuffer);
		free(p_messageString);
		return NULL;
	}

	//Add ':' & advance the number of bytes written to buffer
	*(p_messageString->p_messageBuffer + currentBufferPosition) = ':';
	currentBufferPosition++;
	//Concatenate the First parameter string 
	currentBufferPosition = concatenateStringToStringThatMayContainNullCharacters(p_messageString->p_messageBuffer, p_paramOne, currentBufferPosition, paramOneLength);

	//Concatenate the Second parameter string if it exists
	if (NULL != p_paramTwo) {
		//Add ';' & advance the number of bytes written to buffer
		*(p_messageString->p_messageBuffer + currentBufferPosition) = ';';
		currentBufferPosition++;
		currentBufferPosition = concatenateStringToStringThatMayContainNullCharacters(p_messageString->p_messageBuffer, p_paramTwo, currentBufferPosition, paramTwoLength);
	}

	//Concatenate the Third parameter string if it exists
	if (NULL != p_paramThree) {
		//Add ';' & advance the number of bytes written to buffer
		*(p_messageString->p_messageBuffer + currentBufferPosition) = ';';
		currentBufferPosition++;
		currentBufferPosition = concatenateStringToStringThatMayContainNullCharacters(p_messageString->p_messageBuffer, p_paramThree, currentBufferPosition, paramThreeLength);
	}
	
	//Concatenate the Fourth parameter string if it exists
	if (NULL != p_paramFour) {
		//Add ';' & advance the number of bytes written to buffer
		*(p_messageString->p_messageBuffer + currentBufferPosition) = ';';
		currentBufferPosition++;
		currentBufferPosition = concatenateStringToStringThatMayContainNullCharacters(p_messageString->p_messageBuffer, p_paramFour, currentBufferPosition, paramFourLength);
	}

	//Finally, adding the Carriage Return & Line Feed to the message buffer
	*(p_messageString->p_messageBuffer + currentBufferPosition) = '\r';
	*(p_messageString->p_messageBuffer + currentBufferPosition+1) = '\n';
	*(p_messageString->p_messageBuffer + currentBufferPosition+2) = '\0';


	//Because this type of messages, for either Server or Clients, have NO parameters, 
	//	then the length of the message type string is the length of the entire message!!!
	p_messageString->messageLength = totalLen;

	//Return the allocated & updated "message" struct..
	return p_messageString;
}



//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o Server messages
//---SERVER_MAIN_MENU
static messageString* constructServerMainMenuMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(SERVER_MAIN_MENU))) {
		printf("Error: Failed to construct 'SERVER_MAIN_MENU' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_APPROVED
static messageString* constructServerApprovedMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(SERVER_APPROVED))) {
		printf("Error: Failed to construct 'SERVER_APPROVED' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_DENIED
static messageString* constructServerDeniedMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(SERVER_DENIED))) {
		printf("Error: Failed to construct 'SERVER_DENIED' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_INVITE
static messageString* constructServerInviteMessageString(char* p_paramOne)
{
	messageString* p_messageString = NULL;
	//Assert
	assert(NULL != p_paramOne);

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithParameters(SERVER_INVITE, p_paramOne, NULL, NULL, NULL))) {
		printf("Error: Failed to construct 'SERVER_INVITE' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_SETUP_REQUSET
static messageString* constructServerSetupRequestMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(SERVER_SETUP_REQUSET))) {
		printf("Error: Failed to construct 'SERVER_SETUP_REQUSET' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_PLAYER_MOVE_REQUEST
static messageString* constructServerPlayerMoveRequestMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(SERVER_PLAYER_MOVE_REQUEST))) {
		printf("Error: Failed to construct 'SERVER_PLAYER_MOVE_REQUEST' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_GAME_RESULTS
static messageString* constructServerGameResultsMessageString(char* p_paramOne/*#Bulls*/, char* p_paramTwo/*Cows*/, char* p_paramThree/*other player name*/, char* p_paramFour/*other guess*/)
{
	messageString* p_messageString = NULL;
	//Assert
	assert(NULL != p_paramOne);
	assert(NULL != p_paramTwo);
	assert(NULL != p_paramThree);
	assert(NULL != p_paramFour);


	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithParameters(SERVER_GAME_RESULTS, p_paramOne, p_paramTwo, p_paramThree, p_paramFour))) {
		printf("Error: Failed to construct 'SERVER_GAME_RESULTS' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_WIN
static messageString* constructServerWinMessageString(char* p_paramOne, char* p_paramTwo)
{
	messageString* p_messageString = NULL;
	//Assert
	assert(NULL != p_paramOne);
	assert(NULL != p_paramTwo);

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithParameters(SERVER_WIN, p_paramOne, p_paramTwo, NULL, NULL))) {
		printf("Error: Failed to construct 'SERVER_WIN' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_DRAW
static messageString* constructServerDrawMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(SERVER_DRAW))) {
		printf("Error: Failed to construct 'SERVER_DRAW' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_NO_OPPONENTS
static messageString* constructServerNoOpponentsMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(SERVER_NO_OPPONENTS))) {
		printf("Error: Failed to construct 'SERVER_NO_OPPONENTS' message buffer for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---SERVER_OPPONENT_QUIT
static messageString* constructServerOpponentQuitMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(SERVER_OPPONENT_QUIT))) {
		printf("Error: Failed to construct 'SERVER_OPPONENT_QUIT' for the Server to send to Client.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}




//0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o Client messages
//---CLIENT_REQUEST
static messageString* constructClientRequestMessageString(char* p_paramOne /*User name*/)
{
	messageString* p_messageString = NULL;
	//Assert
	assert(NULL != p_paramOne);

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithParameters(CLIENT_REQUEST, p_paramOne, NULL, NULL, NULL))) {
		printf("Error: Failed to construct 'CLIENT_REQUEST' message buffer for the Client to send to Server.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---CLIENT_VERSUS
static messageString* constructClientVersusMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(CLIENT_VERSUS))) {
		printf("Error: Failed to construct 'CLIENT_VERSUS' message buffer for the Client to send to Server.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---CLIENT_SETUP
static messageString* constructClientSetupMessageString(char* p_paramOne /*Initial self number*/)
{
	messageString* p_messageString = NULL;
	//Assert
	assert(NULL != p_paramOne);

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithParameters(CLIENT_SETUP, p_paramOne, NULL, NULL, NULL))) {
		printf("Error: Failed to construct 'CLIENT_SETUP' message buffer for the Client to send to Server.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---CLIENT_PLAYER_MOVE
static messageString* constructClientPlayerMoveMessageString(char* p_paramOne /*self guess of opponent number*/)
{
	messageString* p_messageString = NULL;
	//Assert
	assert(NULL != p_paramOne);

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithParameters(CLIENT_PLAYER_MOVE, p_paramOne, NULL, NULL, NULL))) {
		printf("Error: Failed to construct 'CLIENT_PLAYER_MOVE' message buffer for the Client to send to Server.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}

//---CLIENT_DISCONNECT
static messageString* constructClientDisconnectMessageString()
{
	messageString* p_messageString = NULL;

	//Allocate Heap memory for a "messageString" struct & Insert the message string..
	if (NULL == (p_messageString = constructMessageStringWithNoParameters(CLIENT_DISCONNECT))) {
		printf("Error: Failed to construct 'CLIENT_DISCONNECT' message buffer for the Client to send to Server.\n");
		return NULL;
	}

	//Return the pointer to the String special struct.... 
	return p_messageString;
}











//0oo0ooo0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o Functions mainly to translate a received message
static message* allocateMemoryForMessageStruct()
{
	message* p_message = NULL;

	//Allocate Heap memory for a "message" struct
	if (NULL == (p_message = (message*)calloc(sizeof(message), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate dynamic memory (Heap) for a 'message' struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}


	//Return the allocated & updated "message" struct..
	return p_message;
}
static parameter* allocateMemoryForParameterStruct(int parameterStringLength, char* p_bufferInitialCopyPosition)
{
	parameter* p_parameter = NULL;

	//Allocate Heap memory for a "parameter" struct
	if (NULL == (p_parameter = (parameter*)calloc(sizeof(parameter), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate dynamic memory (Heap) for a 'parameter' struct.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}

	//Allocate Heap memory for the received parameter string
	if (NULL == (p_parameter->p_parameter = (char*)calloc(sizeof(char), parameterStringLength + 1))) {
		printf("Error: Failed to allocate dynamic memory (Heap) string.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}

	//Insert the parameter string data to "parameter" struct
	concatenateStringToStringThatMayContainNullCharacters(p_parameter->p_parameter, p_bufferInitialCopyPosition, 0, parameterStringLength);


	//Return the allocated "parameter" struct..
	return p_parameter;
}

static int findPositionOffsetOfGivenCharacterInBuffer(char* p_receivedBuffer, int scanStartingPosition,char mark)
{
	//Assert
	assert(NULL != p_receivedBuffer);
	//mark will most likely be ';'
	
	//Scan the received buffer...   
	while (TRUE)
	{
		//Validate if the buffer marker reached the end of the 'message type' field in the buffer data OR the end of a 'message parameter' field
		if ((*(p_receivedBuffer + scanStartingPosition) == '\r') || (*(p_receivedBuffer + scanStartingPosition) == mark)) {//'CHECK' '\n'
			break;
		}
		scanStartingPosition++;  //Advance the buffer's front edge
	}
	//scanStartingPosition has become the position of the 'mark' character
	return scanStartingPosition; 
}

static BOOL extractMessageParameters(char* p_receivedBuffer, message* p_receivedMessageInfo, int messageTypeSerialNumber, int bufferCurrentPosition)
{
	int bufferStartingPosition = 0, paramIndex = 0;;
	parameter* p_next = NULL, *p_current = NULL;
	//Assert
	assert(NULL != p_receivedBuffer);
	assert(NULL != p_receivedMessageInfo);
	assert((SERVER_MAIN_MENU_NUM <= messageTypeSerialNumber) && (CLIENT_DISCONNECT_NUM >= messageTypeSerialNumber));
	assert(0 < bufferCurrentPosition);

	//Insert message type serial number to the message info struct
	p_receivedMessageInfo->messageType = messageTypeSerialNumber;

	

	//Keep scanning for parameters in the received buffer...  (':' for the first parameter & ';' for additional parameters)
	while ((*(p_receivedBuffer + bufferCurrentPosition) == ';') || (*(p_receivedBuffer + bufferCurrentPosition) == ':')) {
		//Update the starting scanning position
		bufferStartingPosition = bufferCurrentPosition;
		//Find next parameter if exists (Pay Attention that only messages that must have at least one parameter arrived at this function)
		bufferCurrentPosition = findPositionOffsetOfGivenCharacterInBuffer(p_receivedBuffer, bufferStartingPosition + 1, ';');
		//Create a "parameter" struct, Update it with the string describing the parameter value & Insert it to the "message" struct
		if (NULL == (p_next = allocateMemoryForParameterStruct(
			bufferCurrentPosition - bufferStartingPosition-1,		/* the difference between the initial byte index in the buffer, and the final byte index of the section */
			p_receivedBuffer + bufferStartingPosition + 1))) {		/* pointer to the address from which the first parameter's section in the string begins */
			
			printf("Error: Failed to create a parameter struct for a received message tp contain one of the message's parameter's info.\n");
			printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no.: %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
			return STATUS_CODE_FAILURE;
		}
		//Update parameters nested list
		if (0 == paramIndex) {
			p_receivedMessageInfo->p_parameters = p_next;	//Starting the parameters next list
			p_current = p_receivedMessageInfo->p_parameters;//pointing a temp pointer at the current last parameter in the list == the first
		}
		else {
			p_current->p_nextParameter = p_next; //Chaining the links (parameters)
			p_current = p_next;				 	 //Advancing to the next link
		}
		//Prepare for more parameters
		paramIndex++;
	}

	//"message" struct updated as needed...
	return STATUS_CODE_SUCCESS;
}

static BOOL extractMessageInfo(char* p_receivedBuffer, message* p_receivedMessageInfo)
{
	int receivedMessageTypeLength = 0;
	char* p_receivedMessageTypeString = NULL;
	//Assert
	assert(NULL != p_receivedBuffer);
	//assert(0 < messageLength); 'CHECK'

	//Find the index of the last character of the received message type part of the buffer  (also #MessageTypeBytes)
	receivedMessageTypeLength = findPositionOffsetOfGivenCharacterInBuffer(p_receivedBuffer, 0, ':');

	//Allocate Heap memory for the received message type string
	if (NULL == (p_receivedMessageTypeString = (char*)calloc(sizeof(char), receivedMessageTypeLength + 1))) {
		printf("Error: Failed to allocate dynamic memory (Heap) string.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}

	/*
	//Insert message type string into a new buffer - NOTE: HERE string function e.g. sprintf_s would work because there are no '\0' data involved.
	///SECOND Note: In fact both sscanf_s & sprintf_s failed to write data from BIGGER buffer to a SMALLER buffer
	// without any unconvertable characters - READ https://docs.microsoft.com/en-us/cpp/c-runtime-library/scanf-width-specification?view=msvc-160 
	if (EOF == sscanf_s(p_receivedBuffer, "%s", p_receivedMessageTypeString, receivedMessageTypeLength)) {
		printf("Error: Failed to copy the message type into a new buffer\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		free(p_receivedMessageTypeString);
		return STATUS_CODE_FAILURE;
	}*/

	//Insert message type string into a new buffer 
	concatenateStringToStringThatMayContainNullCharacters(p_receivedMessageTypeString, p_receivedBuffer, 0, receivedMessageTypeLength);

	//Compare the extracted message type string to all possible types...
	//Server
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_MAIN_MENU, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = SERVER_MAIN_MENU_NUM;
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_APPROVED, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = SERVER_APPROVED_NUM;
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_DENIED, receivedMessageTypeLength + 1)) 
		if (STATUS_CODE_SUCCESS != extractMessageParameters(p_receivedBuffer, p_receivedMessageInfo, SERVER_DENIED_NUM, receivedMessageTypeLength)){
			free(p_receivedMessageTypeString);
			return STATUS_CODE_FAILURE;
		}

	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_INVITE, receivedMessageTypeLength + 1)) 
		if (STATUS_CODE_SUCCESS != extractMessageParameters(p_receivedBuffer, p_receivedMessageInfo, SERVER_INVITE_NUM, receivedMessageTypeLength)) {
			free(p_receivedMessageTypeString);
			return STATUS_CODE_FAILURE;
		}

	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_SETUP_REQUSET, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = SERVER_SETUP_REQUSET_NUM;
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_PLAYER_MOVE_REQUEST, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = SERVER_PLAYER_MOVE_REQUEST_NUM;
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_GAME_RESULTS, receivedMessageTypeLength + 1))  
		if (STATUS_CODE_SUCCESS != extractMessageParameters(p_receivedBuffer, p_receivedMessageInfo, SERVER_GAME_RESULTS_NUM, receivedMessageTypeLength)) {
			free(p_receivedMessageTypeString);
			return STATUS_CODE_FAILURE;
		}

	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_WIN, receivedMessageTypeLength + 1))  
		if (STATUS_CODE_SUCCESS != extractMessageParameters(p_receivedBuffer, p_receivedMessageInfo, SERVER_WIN_NUM, receivedMessageTypeLength)) {
			free(p_receivedMessageTypeString);
			return STATUS_CODE_FAILURE;
		}

	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_DRAW, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = SERVER_DRAW_NUM;
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_NO_OPPONENTS, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = SERVER_NO_OPPONENTS_NUM;
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, SERVER_OPPONENT_QUIT, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = SERVER_OPPONENT_QUIT_NUM;
	
	

	//Client
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, CLIENT_REQUEST, receivedMessageTypeLength + 1)) 
		if (STATUS_CODE_SUCCESS != extractMessageParameters(p_receivedBuffer, p_receivedMessageInfo, CLIENT_REQUEST_NUM, receivedMessageTypeLength)) {
			free(p_receivedMessageTypeString);
			return STATUS_CODE_FAILURE;
		}

	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, CLIENT_VERSUS, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = CLIENT_VERSUS_NUM;
	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, CLIENT_SETUP, receivedMessageTypeLength + 1))  
		if (STATUS_CODE_SUCCESS != extractMessageParameters(p_receivedBuffer, p_receivedMessageInfo, CLIENT_SETUP_NUM, receivedMessageTypeLength)) {
			free(p_receivedMessageTypeString);
			return STATUS_CODE_FAILURE;
		}

	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, CLIENT_PLAYER_MOVE, receivedMessageTypeLength + 1)) 
		if (STATUS_CODE_SUCCESS != extractMessageParameters(p_receivedBuffer, p_receivedMessageInfo, CLIENT_PLAYER_MOVE_NUM, receivedMessageTypeLength)) {
			free(p_receivedMessageTypeString);
			return STATUS_CODE_FAILURE;
		}

	if (STRINGS_ARE_EQUAL(p_receivedMessageTypeString, CLIENT_DISCONNECT, receivedMessageTypeLength + 1))  p_receivedMessageInfo->messageType = CLIENT_DISCONNECT_NUM;

	//Free the message type temporary (used for comparisons only) string
	free(p_receivedMessageTypeString);
	return STATUS_CODE_SUCCESS;
}
