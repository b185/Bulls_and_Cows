/* SetCommmunicationServerSide.h
-------------------------------------------------------------------------
	Module Description - header module for SetCommmunicationServerSide.c
-------------------------------------------------------------------------
*/


#pragma once
#ifndef __SET_COMMUNICATION_SERVER_SIDE_H__
#define __SET_COMMUNICATION_SERVER_SIDE_H__


// Library includes -------------------------------------------------------
#include <Windows.h>



// Projects includes ------------------------------------------------------
#include "HardCodedData.h"


//Functions Declarations

/// <summary>
/// Main connection set and termination of the server process....
/// will create a listening socket bound to a local socket address associated with the input port number.
/// Initiate 'Exit' thread routine. Creates the Server Worker threads to handle communication with incoming clients
/// Will terminate all threads and free memory of the program and 'exit' is inserted or a fatal error occurs
/// </summary>
/// <param name="unsigned short serverPortNumber - port number 0 -65536"></param>
/// <returns>True if operation succeeded, False if otherwise</returns>
BOOL setCommmunicationServerSide(unsigned short serverPortNumber);

#endif //__SERVER_CLIENTS_TOOLS_H__
