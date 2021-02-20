/* FetchAndValidateCommandlineArguments.h
----------------------------------------------------------------------------------
	Module Description - header module for FetchAndValidateCommandlineArguments.c
----------------------------------------------------------------------------------
*/


#pragma once
#ifndef __FETCH_AND_VALIDATE_COMMANDLINE_ARGUMENTS_H__
#define __FETCH_AND_VALIDATE_COMMANDLINE_ARGUMENTS_H__


// Library includes -------------------------------------------------------
#include <Windows.h>



// Projects includes ------------------------------------------------------
#include "HardCodedData.h"


//Functions Declarations

/// <summary>
/// Description - This function receives the command line arguments and translates
///		the strings representing them to values with the desired data types, and places
///		these values into variables addresses that are also received as input. 
///		Evantually, the function chains back to "main" whether the inputs are legal or not.
/// </summary>
/// <param name="char* p_portNumberString - pointer to the commandline string representing the Server port number for both, the Clients & Server"></param>
/// <param name="unsigned short* p_portNumberAddress - pointer to address of the variable that will contain the Server's port number"></param>
/// <param name="char* p_ipAddressString - pointer to the commandline string representing the Server IP address for the Clients - NOT USED IN THIS EXERCISE"></param>
/// <param name="char* p_playerNameString - pointer to the commandline string representing the Client User's name - NOT USED IN THIS EXERCISE"></param>
/// <returns>A BOOL value representing the function's outcome. Success (TRUE) or Failure (FALSE)</returns>
BOOL fetchAndValidateCommandLineArguments(char* p_portNumberString, unsigned short* p_portNumberAddress,
	char* p_ipAddressString, char* p_playerNameString);

#endif //__FETCH_AND_VALIDATE_COMMANDLINE_ARGUMENTS_H__
