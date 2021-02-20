/* SetCommmunicationClientSide.h
-------------------------------------------------------------------------
	Module Description - header module for SetCommmunicationClientSide.c
-------------------------------------------------------------------------
*/


#pragma once
#ifndef __SET_COMMUNICATION_CLIENT_SIDE_H__
#define __SET_COMMUNICATION_CLIENT_SIDE_H__


// Library includes -------------------------------------------------------
#include <Windows.h>



// Projects includes ------------------------------------------------------
#include "HardCodedData.h"
#include "MemoryHandling.h"
#include "ServerClientsTools.h"
#include "ClientSideSpeakerThreadRoutine.h"

//Functions Declarations
BOOL setCommmunicationClientSide(char* p_ipAddressString, unsigned short serverPortNumber, char* p_playerNameString);

#endif //__SET_COMMUNICATION_CLIENT_SIDE_H__
