/* MessagesTransferringTools.h
-----------------------------------------------------------------------
	Module Description - header module for MessagesTransferringTools.c
-----------------------------------------------------------------------
*/


#pragma once
#ifndef __MESSAGE_TRANSFERRING_TOOLS_H__
#define __MESSAGE_TRANSFERRING_TOOLS_H__


// Library includes -------------------------------------------------------
#include <Windows.h>



// Projects includes ------------------------------------------------------
#include "HardCodedData.h"
#include "MemoryHandling.h"
#include "ServerClientsTools.h"

//Functions Declarations

/// <summary>
/// Description - A wrapper that construct a 'messageString' struct containing a buffer of data arranged in following format:
///					"<message type string>:<param list>\n"		 where		"<param list>" = "<param 1>;<param 2>;...;<param 4>"			 if exists...
/// This function constructs the messages sent from Server to Client.
/// </summary>
/// <param name="int messageType - message type identifier number"></param>
/// <param name="char* p_paramOne - param 1 buffer"></param>
/// <param name="char* p_paramTwo - param 2 buffer"></param>
/// <param name="char* p_paramThree - param 3 buffer"></param>
/// <param name="char* p_paramFour - param 4 buffer"></param>
/// <returns>'messageString' pointer containing the prepared string and its size in bytes if successful, and NULL if failed</returns>
messageString* constructMessageForSendingServer(int messageType, char* p_paramOne, char* p_paramTwo, char* p_paramThree, char* p_paramFour);

/// <summary>
/// Description - same as constructMessageForSendingServer(.) but in the direction		Client -->> Server
/// </summary>
// <param name="int messageType - message type identifier number"></param>
/// <param name="char* p_paramOne - param 1 buffer"></param>
/// <returns>'messageString' pointer containing the prepared string and its size in bytes if successful, and NULL if failed</returns>
messageString* constructMessageForSendingClient(int messageType, char* p_paramOne);

/// <summary>
/// Descirption - Translates the received message buffer into separated pieces of information, message type and parmeters
/// uses the function extractMessageInfo(.) for that purpose
/// </summary>
/// <param name="char* p_receivedBuffer - pointer to the received message buffer"></param>
/// <returns>updated 'message' struct pointer filled with the received message informtion (prameters buffer in the nested list, type identifier number), or NULL if failed</returns>
message* translateReceivedMessageToMessageStruct(char* p_receivedBuffer);





//0o0o0oo0o0o0o0o0oUtility functions
/// <summary>
/// Description - function receives a pointer to a string, and calculates its' length (until '\0')
/// </summary>
/// <param name="char* p_stringBuffer - pointer to string"></param>
/// <returns>length integer value</returns>
int fetchStringLength(char* p_stringBuffer);

/// <summary>
///  Description - function a destination buffer pointer, a source buffer pointer, current position in the destination buffer, starting from
///  which, it is needed to copy a given number of bytes -> Brute Force string copy!!!!
/// </summary>
/// <param name="char* p_destBuffer - destination buffer pointer"></param>
/// <param name="char* p_sourceBuffer - source buffer pointer"></param>
/// <param name="int currentPositionOfDestBuffer - current bytes offset in the destination buffer from which the copy will begin(resume)"></param>
/// <param name="int numberOfBytesToWrite - size of copy"></param>
/// <returns>current new bytes-offset-position in the destination buffer that marks the last position a byte was copied to</returns>
int concatenateStringToStringThatMayContainNullCharacters(char* p_destBuffer, char* p_sourceBuffer,
	int currentPositionOfDestBuffer, int numberOfBytesToWrite);
#endif //__MESSAGE_TRANSFERRING_TOOLS_H__
