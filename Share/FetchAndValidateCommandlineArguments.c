/* FetchAndValidateCommandlineArguments.c
---------------------------------------------------------------------------------
	Module Description - This module contains functions meant for validating the 
		integrity of the Client\Server processes properties from the 
		commandline arguments. 
		e.g. Player name, server's port number, server's IP address.
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
#include "FetchAndValidateCommandlineArguments.h"

// Constants
static const BOOL STATUS_CODE_FAILURE = FALSE;
static const BOOL STATUS_CODE_SUCCESS = TRUE;


// Functions declerations ------------------------------------------------------

/// <summary>
/// Description - This function receives a command line argument of an integer number string
/// and translates it to an intger, and places this value into a variable address, that is also received as input.
/// </summary>
/// <param name="char* p_commandLineString - A pointer to the commandline argument (string) that represents an integer number"></param>
/// <param name="long* p_argumentNumberAddress - A pointer to the number's integer variable's address"></param>
/// <returns>A BOOL value representing the function's outcome (conversion). Success (TRUE) or Failure (FALSE)</returns>
static BOOL fetchArgumentNumber(char* p_commandLineString, long* p_argumentNumberAddress);




// Functions definitions -------------------------------------------------------

BOOL fetchAndValidateCommandLineArguments(char* p_portNumberString, unsigned short* p_portNumberAddress,
	char* p_ipAddressString, char* p_playerNameString)
{
	long portNumIntegrityValidationVariable = 0;
	//Input integrity validation
	if ((NULL == p_portNumberString) || (NULL == p_portNumberAddress)) {
		printf("Error: Bad inputs to function: %s\n", __func__);   return STATUS_CODE_FAILURE;
	}
	


	//Fetching the inserted Port Number in the Server's side that either:									0o0o0o0 BOTH 0o0o0o0
	//	The(a) Client will need to connect to...
	//	The Server will use to set a socket for listening for incoming potential connecting Clients...
	//and validating it is legit
	if (STATUS_CODE_SUCCESS != fetchArgumentNumber(p_portNumberString, &portNumIntegrityValidationVariable)) {
		printf("Error: Failed to read the number of tasks value from commandline.\n");
		return STATUS_CODE_FAILURE;
	}
	//Validating the Server's Port Number is in the "unsgined short" datatype range (1-65536)
	if ((portNumIntegrityValidationVariable <= 0) && (portNumIntegrityValidationVariable >= USHRT_MAX)) {
		printf("Error: Failed to receive a valid port number to operate.\n");
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}
	//Inserting the port number into the designated variable using its' address
	*p_portNumberAddress = (unsigned short)portNumIntegrityValidationVariable;



	





	//Retrieving arguments values was successful...
	return STATUS_CODE_SUCCESS;
}



//......................................Static functions..........................................

static BOOL fetchArgumentNumber(char* p_commandLineString, long* p_argumentNumberAddress)
{
	errno_t retVal;
	//Asserts
	assert(p_commandLineString != NULL);
	assert(p_argumentNumberAddress != NULL);

	//Scanning the string-number from the commandline argument in to an integer address
	if ((retVal = sscanf_s(p_commandLineString, "%ld", p_argumentNumberAddress)) == EOF) {
		printf("At file: %s\nAt line number: %d\nAt function: %s\n\n\n", __FILE__, __LINE__, __func__);
		return STATUS_CODE_FAILURE;
	}

	//Key was retrieved successfuly and is legitimate
	return STATUS_CODE_SUCCESS;
}
