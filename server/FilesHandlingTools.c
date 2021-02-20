/* FilesHandlingTools.c
--------------------------------------------------------------------------------------
	Module Description - This module contains functions meant for transferring 
		information\data through GameSession.txt file (resource) in a predetermined
		Writing\Reading procedure of: 
			<Length of data - 4 bytes><Buffer - Length-of-data bytes>

		It also contains the "1st Player" Writing-Reading wrapperS & "2nd Player"
		Writing-Reading wrapper, when "1st & 2nd" are defined anew for every
		phase the players access the file.
		These wrappers will suit the following chronological manner:
		<1st Player Write>  ->   <2nd Player Read-then-Write>  ->  <1st Player Read>
--------------------------------------------------------------------------------------
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
#include "FilesHandlingTools.h"
#include "MemoryHandling.h"
#include "ServerClientsTools.h"
#include "MessagesTransferringTools.h"

// Constants --------------------------------------------------------------------
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;

static const int SINGLE_OBJECT = 1;

//Mutex
static const long GAME_SESSION_FILE_MUTEX_OWNERSHIP_TIMEOUT = 2200; // 2.2 Seconds - GameSession.txt timeout
static const BOOL MUTEX_OWNERSHIP_RELEASE_FAILED = 0;


//files constants
static const BOOL STATUS_FILE_WRITING_FAILED = (BOOL)0;


// Functions declerations ------------------------------------------------------

/// <summary>
/// Description - A function meant to either Create "GameSession.txt" file if non existent, or Open it. With appropriate inputted Creation Disposition
/// value, the function can also open it & overwrite it. This function will signal the Server's "ERROR" event if any FATAL ERROR occurs e.g mem alloc.
/// </summary>
/// <param name="char* p_filePath - Relative file path (string) of GameSession.txt file"></param>
/// <param name="DWORD creationDisposition - Handle creation method - OPEN EXISTING, CREATE ALWAYS etc."></param>
/// <param name="int playerId - which player entered, 1st or 2nd, considering some phase of the communication - for printing purposes"></param>
/// <param name="HANDLE* p_h_errorEvent - pointer to 'ERROR' event Handle"></param>
/// <returns>pointer to mem alloc. Handle to file, or NULL if failed</returns>
static HANDLE* openFileForReadingAndWritingWithErrorEventScenario(char* p_filePath, DWORD creationDisposition, int playerId, HANDLE* p_h_errorEvent);


/// <summary>
/// Description - A function that wrapps a predetermined Writing procedure to "GameSession.txt" to transfer data.
///	Every writing occurs in two phases. First, the number of bytes of data will be written in the first 4 bytes because 
/// the  size of a DWORD\LONG\long is 4 bytes, then, from that exact position, the data will be written, such that any Reader
/// will know to ALWAYS read the first 4 bytes, and use the size described in that buffer to read the data that starts a byte-position=4 
///
/// </summary>
/// <param name="HANDLE* p_h_gameSessionsFile - pointer to the Handle to GameSession.txt file"></param>
/// <param name="LPTSTR p_dataString - pointer to buffer of bytes containing the data to write to the file (not neccessarily a string)"></param>
/// <param name="DWORD totalStringSizeInBytes - number of bytes of data to write to the file"></param>
/// <returns>True if successful,   False otherwise</returns>
static BOOL writeToFile(HANDLE* p_h_gameSessionsFile, LPTSTR p_dataString, DWORD totalStringSizeInBytes);

/// <summary>
/// Description - Similarly to writeToFile(.) function above, this wrapper function wrapps a predetermined Reading procedure from "GameSession.txt".
/// Every reading occurs in the same two-phase procedure -> first read the first 4 bytes containing the information of the number of bytes of the data,
/// then read the data into a buffer.
/// </summary>
/// <param name="HANDLE* p_h_gameSessionsFile - pointer to the Handle to GameSession.txt file"></param>
/// <returns>pointer to the buffer containing the read data, which may be the players names, their initial numbers or their guess numbers</returns>
static LPSTR readFromFile(HANDLE* p_h_gameSessionsFile);


// Functions definitions -------------------------------------------------------





BOOL firstToReachTheFileWriteWrapper(workingThreadPackage* p_threadInputs, char* p_dataToBeTransferredToOtherPlayerBuffer, DWORD creationDisposition)
{
	HANDLE* p_h_gameSessionFile = NULL;
	//Input integrity validation
	if ((NULL == p_threadInputs) || (NULL == p_dataToBeTransferredToOtherPlayerBuffer)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return STATUS_CODE_FAILURE;
	}

	//Allocate dynamic memory for a Handle object
	if (NULL == (p_h_gameSessionFile = openFileForReadingAndWritingWithErrorEventScenario(
		GAME_SESSION_PATH,					/* "GameSession.txt" relative file path */
		creationDisposition,				/* open the pre-created "GameSession.txt" file or create it */
		1,									/* this is a wrapper for writing for the first player that accesses the file this round */
		p_threadInputs->p_h_errorEvent))) {	/* "ERROR" event handle pointer */
		return STATUS_CODE_FAILURE;
	}

	//Perform writing the buffer representing data concerning the game's data e.g. players names, players guesses
	if (STATUS_CODE_FAILURE == writeToFile(
		p_h_gameSessionFile,													/*"GameSession.txt" file handle*/
		(LPSTR)p_dataToBeTransferredToOtherPlayerBuffer,						/*buffer pointer*/
		(DWORD)fetchStringLength(p_dataToBeTransferredToOtherPlayerBuffer))) {	/*length of the data in buffer*/
		//maybe free(p_dataToBeTransferredToOtherPlayerBuffer);
		return STATUS_CODE_FAILURE;
	}

	//Close the "GameSession.txt" file handle
	closeHandleProcedure(p_h_gameSessionFile);
	return STATUS_CODE_SUCCESS;
}

char* firstToReachTheFileReadWrapper(workingThreadPackage* p_threadInputs)
{
	HANDLE* p_h_gameSessionFile = NULL;
	LPSTR p_readDataBufferFromOtherPlayer = NULL;
	//Input integrity validation
	if (NULL == p_threadInputs) {
		printf("Error: Bad inputs to function: %s\n", __func__); return NULL;
	}

	//Allocate dynamic memory for a Handle object
	if (NULL == (p_h_gameSessionFile = openFileForReadingAndWritingWithErrorEventScenario(
		GAME_SESSION_PATH,					/* "GameSession.txt" relative file path */
		OPEN_EXISTING,						/* open the pre-created "GameSession.txt" file */
		1,									/* this is a wrapper for reading for the first player that accesses the file this round (meaning after write-read-write operations have already happened) */
		p_threadInputs->p_h_errorEvent))) {	/* "ERROR" event handle pointer */
		return NULL;
	}

	//Perform writing the buffer representing data concerning the game's data e.g. players names, players guesses
	if (NULL == (p_readDataBufferFromOtherPlayer = readFromFile(p_h_gameSessionFile	/*"GameSession.txt" file handle*/)))
		return NULL;
	

	//Close the "GameSession.txt" file handle
	closeHandleProcedure(p_h_gameSessionFile);
	//Return the read buffer string
	return (char*)p_readDataBufferFromOtherPlayer;
}






char* secondToReachTheFileReadThenWriteWrapper(workingThreadPackage* p_threadInputs, char* p_dataToBeTransferredToOtherPlayerBuffer)
{
	HANDLE* p_h_gameSessionFile = NULL;
	LPSTR p_readDataBufferFromOtherPlayer = NULL;
	//Input integrity validation
	if ((NULL == p_threadInputs) || (NULL == p_dataToBeTransferredToOtherPlayerBuffer)) {
		printf("Error: Bad inputs to function: %s\n", __func__); return NULL;
	}

	//Allocate dynamic memory for a Handle object
	if (NULL == (p_h_gameSessionFile = openFileForReadingAndWritingWithErrorEventScenario(
		GAME_SESSION_PATH,					/* "GameSession.txt" relative file path */
		OPEN_EXISTING,						/* open the pre-created "GameSession.txt" file */
		0,									/* this is a wrapper for reading & then writing for the second player that accesses the file this round */
		p_threadInputs->p_h_errorEvent))) {	/* "ERROR" event handle pointer */
		return NULL;
	}




	//First, it is needed to read the data written by the first player
	//Perform writing the buffer representing data concerning the game's data e.g. players names, players guesses
	if (NULL == (p_readDataBufferFromOtherPlayer = readFromFile(p_h_gameSessionFile	/*"GameSession.txt" file handle*/)))
		return NULL;

	//Second, write to the file the data we need to transfer to the first player, by RUNNING OVER the data the first player wrote
	//Perform writing the buffer representing data concerning the game's data e.g. players names, players guesses
		if (STATUS_CODE_FAILURE == writeToFile(
			p_h_gameSessionFile,													/*"GameSession.txt" file handle*/
			(LPSTR)p_dataToBeTransferredToOtherPlayerBuffer,						/*buffer pointer*/
			(DWORD)fetchStringLength(p_dataToBeTransferredToOtherPlayerBuffer))) {	/*length of the data in buffer*/
			//operation failed -> free the data received from 1st player
			free(p_readDataBufferFromOtherPlayer);
			return NULL;
		}




	//Close the "GameSession.txt" file handle
	closeHandleProcedure(p_h_gameSessionFile);
	return p_readDataBufferFromOtherPlayer;

}




BOOL fileTruncationForWhenGameEnds(workingThreadPackage* p_threadInputs)
{
	HANDLE* p_h_gameSessionFile = NULL;
	//Input integrity validation
	if (NULL == p_threadInputs) {
		printf("Error: Bad inputs to function: %s\n", __func__); return STATUS_CODE_FAILURE;
	}

	//Allocate dynamic memory for a Handle object and open the file in CREATE_ALWAYS to erase its contents!!!
	if (NULL == (p_h_gameSessionFile = openFileForReadingAndWritingWithErrorEventScenario(
		GAME_SESSION_PATH,					/* "GameSession.txt" relative file path */
		CREATE_ALWAYS,						/* use CREATE_ALWAYS to erase to contents of the file for the following game session */
		1,									/* don't care (according to moodle instructions clarifications) */
		p_threadInputs->p_h_errorEvent))) {	/* "ERROR" event handle pointer */
		return STATUS_CODE_FAILURE;
	}


	//Immediately close the "GameSession.txt" file handle after creating it. It has no need without creating it for truncating
	closeHandleProcedure(p_h_gameSessionFile);
	//Return the read buffer string
	return STATUS_CODE_SUCCESS;
}





//......................................Static functions..........................................

static HANDLE* openFileForReadingAndWritingWithErrorEventScenario(char* p_filePath, DWORD creationDisposition, int playerId, HANDLE* p_h_errorEvent)
{
	HANDLE* p_h_fileHandle = NULL;
	//Assert
	assert(NULL != p_filePath);

	//Allocating dynamic memory (Heap) for a file Handle pointer
	if (NULL == (p_h_fileHandle = (HANDLE*)calloc(sizeof(HANDLE), SINGLE_OBJECT))) {
		printf("Error: Failed to allocate memory for a Handle to file '%s'.\n", p_filePath);
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*p_h_errorEvent)) {  //reason: Second Player Event was not set on time by First Player
			printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
			//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
		}
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return  NULL;
	}

	//Open file for reading
	*p_h_fileHandle = CreateFile(
		p_filePath,							// Const null - terminated string describing the file's path
		GENERIC_READ | GENERIC_WRITE,		// Desired Access is set to Reading mode 
		FILE_SHARE_READ | FILE_SHARE_WRITE,	// Share Mode:Here various threads may read from the input file (input message)
		NULL,								// No Security Attributes
		creationDisposition,				// The GameSession.txt may be an existing file or a file we need to create.
		FILE_ATTRIBUTE_NORMAL,				// General reading in files 
		NULL								// No Template
	);
	//File Handle creation validation
	if (INVALID_HANDLE_VALUE == *p_h_fileHandle) {
		if (playerId == 1) printf("Error: Failed to create a Handle to GameSession.txt file by creating the file by 1st Player, with code: %d.\n", GetLastError());
		else printf("Error: Failed to create a Handle to GameSession.txt file by opening the file by 2nd Player, with code: %d.\n", GetLastError());
		if (SET_EVENT_TO_SIGNALED_STATE_FAILED == SetEvent(*p_h_errorEvent)) {  //reason: Second Player Event was not set on time by First Player
			printf("Error: Failed to set 'ERROR' event to signaled state for ALL threads to be notified to finish, with error code no. %ld.\nExiting..\n\n", GetLastError());
			//Continue to exiting regardless of failure return STATUS_CODE_FAILURE;
		}
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		free(p_h_fileHandle);
		return NULL;
	}
	//Returning a pointer to the created handle to file
	return p_h_fileHandle;
}


static BOOL writeToFile(HANDLE* p_h_gameSessionsFile, LPTSTR p_dataString, DWORD totalStringSizeInBytes)
{
	DWORD retValSet = 0, numberOfBytesWritten = 0;
	BOOL retValWrite = FALSE;
	//Asserts
	assert(p_h_gameSessionsFile != NULL);
	assert(p_dataString != NULL);

	/*..........................*/
	/* Write the size of buffer */
	/*..........................*/
	//TO PREVENT MISTAKES we assure the handle is located at the beginning of the file. (in the case of read after write!)
	// Set the Handle to point at the byte position from which the print to the file should start - beginning of file!!
	retValSet = SetFilePointer(
		*p_h_gameSessionsFile,				//Tasks List file Handle 
		0,									//Initial byte position set to be after the first 4 bytes of the size of the data buffer
		NULL,								//Assuming there is less than 2^32 bytes in the input file -recheck
		FILE_BEGIN							//Starting byte count is set to the start of the file which constatnly changes
	);
	//Validate Handle pointing succeeded...
	if (INVALID_SET_FILE_POINTER == retValSet) {
		//Initial byte position of wasn't found
		printf("Error: Failed to reset the file Handle pointer position for printing, with code: %d.\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return STATUS_CODE_FAILURE;
	}
	//Writing to file the size of the data at the first 4 bytes at the desired position
	retValWrite = WriteFile(
		*p_h_gameSessionsFile,					//GameSession.txt Handle 
		(char*)(&totalStringSizeInBytes),		//Data length buffer to be written first and indicate the reader how many bytes he needs to read (explicit typecast to buffer of bytes)
		sizeof(totalStringSizeInBytes),			//Total number of bytes, needed to be written
		&numberOfBytesWritten,					//Pointer to the total number of bytes written 
		NULL									//Overlapped - off
	);
	if (STATUS_FILE_WRITING_FAILED == retValWrite) {
		//Failed to write the number of bytes to be written to file
		printf("Error: Failed to write the number of bytes to be written to the file Handle. Exited with code: %d\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return STATUS_CODE_FAILURE;
	}



	/*..........................*/
	/* Write the data in buffer */
	/*..........................*/
	//TO PREVENT MISTAKES we assure the handle is located in the right place for writting the data.
	// Set the Handle to point at the byte position from which the print to the file should start - 4 bytes after the start
	retValSet = SetFilePointer(
		*p_h_gameSessionsFile,				//Tasks List file Handle 
		numberOfBytesWritten/*was 4*/,		//Initial byte position set to be after the first 4 bytes of the size of the data buffer - move forward in the file is granted for values greater than 0
		NULL,								//Assuming there is less than 2^32 bytes in the input file 
		FILE_BEGIN							//Starting byte count is set to the start of the file which constatnly changes
	);
	//Validate Handle pointing succeeded...
	if (INVALID_SET_FILE_POINTER == retValSet) {
		//Initial byte position of wasn't found
		printf("Error: Failed to reset the file Handle pointer position for printing, with code: %d.\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return STATUS_CODE_FAILURE;
	}


	//Writing to Tasks List file at the desired position
	retValWrite = WriteFile(
		*p_h_gameSessionsFile,					//GameSession.txt Handle set after the first 4 bytes
		p_dataString,							//Data string buffer pointer
		totalStringSizeInBytes,					//Length of buffer in bytes, needed to be written
		&numberOfBytesWritten,					//Pointer to the total number of bytes written 
		NULL									//Overlapped - off
	);
	if (STATUS_FILE_WRITING_FAILED == retValWrite) {
		//Failed to write the needed memory from the file
		printf("Error: Failed to write to the file Handle. Exited with code: %d\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return STATUS_CODE_FAILURE;
	}

	//Writing to output file was successful...
	return STATUS_CODE_SUCCESS;
}


static LPSTR readFromFile(HANDLE* p_h_gameSessionsFile)
{
	DWORD retValSet = 0, numberOfBytesRead = 0, totalStringSizeInBytes = 0;
	BOOL retValWrite = FALSE;
	LPTSTR p_readDataString = NULL;
	//Asserts
	assert(p_h_gameSessionsFile != NULL);

	/*.........................*/
	/* Read the size of buffer */
	/*.........................*/
	//TO PREVENT MISTAKES we assure the handle is located at the beginning of the file. (in the case of read after write!)
	// Set the Handle to point at the byte position from which the print to the file should start - beginning of file!!
	retValSet = SetFilePointer(
		*p_h_gameSessionsFile,				//GameSession.txt file Handle 
		0,									//Initial byte position set to be after the first 4 bytes of the size of the data buffer
		NULL,								//Assuming there is less than 2^32 bytes in the input file -recheck
		FILE_BEGIN							//Starting byte count is set to the start of the file which constatnly changes
	);
	//Validate Handle pointing succeeded...
	if (INVALID_SET_FILE_POINTER == retValSet) {
		//Initial byte position of wasn't found
		printf("Error: Failed to reset the file Handle pointer position for printing, with code: %d.\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}
	//Reading from file the size of the data at the first 4 bytes at the desired position
	retValWrite = ReadFile(
		*p_h_gameSessionsFile,					//GameSession.txt Handle 
		(char*)(&totalStringSizeInBytes),		//Data length buffer to be read first and indicate the reader how many bytes he needs to read  (explicit typecast to buffer of bytes)
		sizeof(totalStringSizeInBytes),			//Total number of bytes, needed to be written
		&numberOfBytesRead,						//Pointer to the total number of bytes written 
		NULL									//Overlapped - off
	);
	if (STATUS_FILE_WRITING_FAILED == retValWrite) {
		//Failed to write the number of bytes to be written to file
		printf("Error: Failed to read the number of bytes to be read to the file Handle. Exited with code: %d\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}


	//Allocating dynamic memory (Heap) for a buffer data(string)
	if (NULL == (p_readDataString = (TCHAR*)calloc(sizeof(TCHAR), totalStringSizeInBytes + 1))) {
		printf("Error: Failed to allocate memory for a the buffer that will contain the data that is supposed to be read,.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Worker thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return  NULL;
	}

	/*.........................*/
	/* Read the data in buffer */
	/*.........................*/
	//TO PREVENT MISTAKES we assure the handle is located in the right place for writting the data.
	// Set the Handle to point at the byte position from which the print to the file should start -  4 bytes after the start
	retValSet = SetFilePointer(
		*p_h_gameSessionsFile,				//Tasks List file Handle 
		numberOfBytesRead/*4*/,				//Initial byte position set to be after the first 4 bytes of the size of the data buffer
		NULL,								//Assuming there is less than 2^32 bytes in the input file -recheck
		FILE_BEGIN							//Starting byte count is set to offset 4B from the start of the file which constatnly changes
	);
	//Validate Handle pointing succeeded...
	if (INVALID_SET_FILE_POINTER == retValSet) {
		//Initial byte position of wasn't found
		printf("Error: Failed to reset the file Handle pointer position for reading, with code: %d.\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}


	//Reading from "GameSession.txt" the data from the desired position
	retValWrite = ReadFile(
		*p_h_gameSessionsFile,					//GameSession.txt Handle set after the first 4 bytes
		p_readDataString,						//Data string buffer pointer
		totalStringSizeInBytes,					//Length of buffer in bytes, needed to be read
		&numberOfBytesRead,						//Pointer to the total number of bytes read 
		NULL									//Overlapped - off
	);
	if (STATUS_FILE_WRITING_FAILED == retValWrite) {
		//Failed to read the needed memory from the file
		printf("Error: Failed to read to the file Handle. Exited with code: %d\n", GetLastError());
		printf("At file: %s\nAt line number: %d\nAt function: %s\nBy Thread no. %ld\n\n\n", __FILE__, __LINE__, __func__, GetCurrentThreadId());
		return NULL;
	}


	*(p_readDataString + totalStringSizeInBytes) = '\0'; //Assuring it is a string

	//Reading the buffer from file was successful...
	return p_readDataString;
}
