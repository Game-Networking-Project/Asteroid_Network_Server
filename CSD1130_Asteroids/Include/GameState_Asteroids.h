/******************************************************************************/
/*!
\file		GameState_Asteroids.h
\author 	Cheong Jia Zen, jiazen.c, 2301549
\par    	jiazen.c@digipen.edu
\date   	February 06, 2024
\brief		This file contains the declaration of 6 functions needed for
			state GS-ASTEROID. They are:
			GameStateAsteroidsLoad();
			GameStateAsteroidsInit();
			GameStateAsteroidsUpdate();
			GameStateAsteroidsDraw();
			GameStateAsteroidsFree();
			GameStateAsteroidsUnload();
			This 5 function below is declare and define in the GameState_Asteroids.cpp file
			gameObjInstCreate ();
			gameObjInstDestroy();
			Helper_Wall_Collision();
			Random_value_Generator();
			Random_number_asteroid_generator();

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#ifndef CSD1130_GAME_STATE_PLAY_H_
#define CSD1130_GAME_STATE_PLAY_H_

// ---------------------------------------------------------------------------

void GameStateAsteroidsLoad(void);
void GameStateAsteroidsInit(void);
void GameStateAsteroidsUpdate(void);
void GameStateAsteroidsDraw(void);
void GameStateAsteroidsFree(void);
void GameStateAsteroidsUnload(void);

// ---------------------------------------------------------------------------

#endif // CSD1130_GAME_STATE_PLAY_H_


