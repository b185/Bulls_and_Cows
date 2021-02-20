/* ClientSideSpeakerThreadRoutine.h
----------------------------------------------------------------------------
	Module Description - header module for ClientSideSpeakerThreadRoutine.c
----------------------------------------------------------------------------
*/


#pragma once
#ifndef __CLIENT_SIDE_SPEAKER_THREAD_ROUTINE_H__
#define __CLIENT_SIDE_SPEAKER_THREAD_ROUTINE_H__


// Library includes -------------------------------------------------------
#include <Windows.h>



// Projects includes ------------------------------------------------------
#include "MemoryHandling.h"
#include "ServerClientsTools.h"
#include "MessagesTransferringTools.h"
#include "HardCodedData.h"

//Functions Declarations

/// <summary>
/// Main routine of a Client Speaker thread to communicates directly with a Worker Server thread
/// </summary>
/// <param name="lpParam - meant to become threads inputs structure 'client...'"></param>
/// <returns>'communicationResults' code according to all of the Exit codes possible</returns>
communicationResults WINAPI clientSideSpeakerThreadRoutine(LPVOID lpParam);

#endif //__CLIENT_SIDE_SPEAKER_THREAD_ROUTINE_H__
