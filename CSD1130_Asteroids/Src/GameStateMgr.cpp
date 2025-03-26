/******************************************************************************/
/*!
\file		GameStateMgr.cpp
\author 	Cheong Jia Zen, jiazen.c, 2301549
\par    	jiazen.c@digipen.edu
\date   	February 06, 2024
\brief		This file contains the definition of two functions 
			GameStateMgrInit() and GameStateMgrUpdate().

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"

// ---------------------------------------------------------------------------
// globals

// variables to keep track the current, previous and next game state
unsigned int	gGameStateInit;
unsigned int	gGameStateCurr;
unsigned int	gGameStatePrev;
unsigned int	gGameStateNext;

// pointer to functions for game state life cycles functions
void (*GameStateLoad)()		= 0;
void (*GameStateInit)()		= 0;
void (*GameStateUpdate)()	= 0;
void (*GameStateDraw)()		= 0;
void (*GameStateFree)()		= 0;
void (*GameStateUnload)()	= 0;

/******************************************************************************/
/*!
	GameStateMgrInit() initialize the game state to starting state.
	It should be called after the program has been started.
*/
/******************************************************************************/
void GameStateMgrInit(unsigned int gameStateInit)
{
	// set the initial game state
	gGameStateInit = gameStateInit;

	// reset the current, previoud and next game
	gGameStateCurr = 
	gGameStatePrev = 
	gGameStateNext = gGameStateInit;

	// call the update to set the function pointers
	GameStateMgrUpdate();
}

/******************************************************************************/
/*!
	GameStateMgrUpdate() updates the game state by changing it's function pointers
	pointing to the corresponding function for the current game state.
	It should be called when game state changed.
*/
/******************************************************************************/
void GameStateMgrUpdate()
{
	if ((gGameStateCurr == GS_RESTART) || (gGameStateCurr == GS_QUIT))
		return;

	switch (gGameStateCurr)
	{
	// let the pointer to point to the relevant function
	case GS_ASTEROIDS:
		GameStateLoad = GameStateAsteroidsLoad;
		GameStateInit = GameStateAsteroidsInit;
		GameStateUpdate = GameStateAsteroidsUpdate;
		GameStateDraw = GameStateAsteroidsDraw;
		GameStateFree = GameStateAsteroidsFree;
		GameStateUnload = GameStateAsteroidsUnload;
		break;
	case GS_RESTART:
		break;
	case GS_QUIT:
		break;
	default:
		AE_FATAL_ERROR("invalid state!!");
	}
}
