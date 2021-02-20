/* FilesHandlingTools.h
---------------------------------------------------------------
	Module Description - header module for FilesHandlingTools.c
----------------------------------------------------------------
*/


#pragma once
#ifndef __FILES_HANDLING_TOOLS_H__
#define __FILES_HANDLING_TOOLS_H__


// Library includes -------------------------------------------------------
#include <Windows.h>



// Projects includes ------------------------------------------------------
#include "HardCodedData.h"
#include "MemoryHandling.h"
#include "ServerClientsTools.h"
#include "MessagesTransferringTools.h"

//Functions Declarations

/// <summary>
/// Description - This is the first-of-two wrapper accessing to "GameSession.txt" file of the "1st Player". It always Writes-only.
/// It is named "1st Player" because it indicates the first ARRIVER to the file accessing current phase. 
/// </summary>
/// <param name="workingThreadPackage* p_threadInputs - current threads inputs - Pointers, Handles, Socket"></param>
/// <param name="char* p_dataToBeTransferredToOtherPlayerBuffer - buffer of bytes meant to be written to the file Before reading (by  the other wrapper)"></param>
/// <param name="DWORD creationDisposition - since this function will be the first accessing the file in the Server session, it should be able to create the file from scratch"></param>
/// <returns>True if successful, False otherwise</returns>
BOOL firstToReachTheFileWriteWrapper(workingThreadPackage* p_threadInputs, char* p_dataToBeTransferredToOtherPlayerBuffer, DWORD creationDisposition);

/// <summary>
/// Description - This is the second-of-two wrapper accessing to "GameSession.txt" file of the "1st Player".It always Reads-only.
/// It is named "1st Player" because it indicates the first ARRIVER to the file accessing current phase. 
/// </summary>
/// <param name="workingThreadPackage* p_threadInputs - current threads inputs - Pointers, Handles, Socket"></param>
/// <returns>pointer to the read buffer of data transferred by the second ARRIVER player if successful, or NULL if failure taken place</returns>
char* firstToReachTheFileReadWrapper(workingThreadPackage* p_threadInputs);

///'CHECK'
BOOL firstToReachTheFileReadWrapperWithMutexLockAndRelease(workingThreadPackage* p_threadInputs, int dataTypeBit);


/// <summary>
/// Description - This is the wrapper accessing to "GameSession.txt" file of the "2nd Player". It will always follow the procedure of
/// "Read-then-Write", when this player is considered the Second ARRIVER player at any file accessing phase.
/// The function will create a handle to the file, and access it, after the "1st P" "2nd P" Events Synchornization procedure had taken place.
/// </summary>
/// <param name="workingThreadPackage* p_threadInputs - current threads inputs - Pointers, Handles, Socket"></param>
/// <param name="char* p_dataToBeTransferredToOtherPlayerBuffer - buffer of bytes meant to be written to the file after finishing reading"></param>
/// <returns>pointer to the read buffer of data transferred by the first ARRIVER player if successful, or NULL if failure taken place</returns>
char* secondToReachTheFileReadThenWriteWrapper(workingThreadPackage* p_threadInputs, char* p_dataToBeTransferredToOtherPlayerBuffer);




/// <summary>
/// Description - This function opens "GameSession.txt" with CREATE_ALWAYS "creation disposition" and immediately closes it.
/// This assures the files' contents are getting erased for the next gaming session!!!
/// </summary>
/// <param name="workingThreadPackage* p_threadInputs - thread's inputs that will allow the signaling of the 'ERROR' event in any fatal error"></param>
/// <returns>True if succeeded. False otherwise</returns>
BOOL fileTruncationForWhenGameEnds(workingThreadPackage* p_threadInputs);


#endif //__FILES_HANDLING_TOOLS_H__
