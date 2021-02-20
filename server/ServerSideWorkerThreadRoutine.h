/* ServerSideWorkerThreadRoutine.h
----------------------------------------------------------------------------
	Module Description - header module for ServerSideWorkerThreadRoutine.c
----------------------------------------------------------------------------
*/


#pragma once
#ifndef __SERVER_SIDE_WORKER_THREAD_ROUTINE_H__
#define __SERVER_SIDE_WORKER_THREAD_ROUTINE_H__


// Library includes -------------------------------------------------------
#include <Windows.h>



// Projects includes ------------------------------------------------------
#include "HardCodedData.h"


//Functions Declarations

/// <summary>
/// Main routine of a Server Worker thread to communicates directly with a Speaker Client thread
/// </summary>
/// <param name="lpParam - meant to become threads inputs structure 'working...'"></param>
/// <returns>'communicationResults' code according to all of the Exit codes possible</returns>
communicationResults WINAPI serverSideWorkerThreadRoutine(LPVOID lpParam);

#endif //__SERVER_SIDE_WORKER_THREAD_ROUTINE_H__
