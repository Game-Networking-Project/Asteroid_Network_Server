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
#include <map>
#include <mutex>

#include "Collision.h"
#include "EngineUtility.h"
// ---------------------------------------------------------------------------


struct AsteroidData;
const unsigned int	SHIP_INITIAL_NUM = 3;			// initial number of ship lives
const float			SHIP_SCALE_X = 16.0f;		// ship scale x
const float			SHIP_SCALE_Y = 16.0f;		// ship scale y
const float			BULLET_SCALE_X = 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y = 3.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE_X = 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X = 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y = 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y = 60.0f;		// asteroid maximum scale y

const float			WALL_SCALE_X = 64.0f;		// wall scale x
const float			WALL_SCALE_Y = 164.0f;		// wall scale y

const float			SHIP_ACCEL_FORWARD = 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD = 100.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED = (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED = 400.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

static bool			onValueChange = false;

// -----------------------------------------------------------------------------
enum TYPE
{
	// list of game object types
	TYPE_SHIP = 0,
	TYPE_BULLET,
	TYPE_ASTEROID,
	TYPE_WALL,

	TYPE_NUM
};

// -----------------------------------------------------------------------------
// object flag definition

const unsigned long FLAG_ACTIVE = 0x00000001;

/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
	unsigned long		type;		// object type
	AEGfxVertexList* pMesh;		// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
	GameObj* pObject;	// pointer to the 'original' shape
	unsigned long		flag;		// bit flag or-ed together
	AEVec2				scale;		// scaling value of the object instance
	AEVec2				posCurr;	// object current position

	AEVec2				posPrev;	// object previous position -> it's the position calculated in the previous loop

	AEVec2				velCurr;	// object current velocity
	float				dirCurr;	// object current direction
	AABB				boundingBox;// object bouding box that encapsulates the object
	AEMtx33				transform;	// object transformation matrix: Each frame, 
	// calculate the object instance's transformation matrix and save it here

};


struct GameObjDataInstance
{
	
	TYPE objectType;
	unsigned long positionInServerDB;
	

	unsigned long		flag;		// bit flag or-ed together
	AEVec2				scale;		// scaling value of the object instance
	AEVec2				posCurr;	// object current position

	AEVec2				posPrev;	// object previous position -> it's the position calculated in the previous loop

	AEVec2				velCurr;	// object current velocity
	float				dirCurr;	// object current direction
	AABB				boundingBox;// object bouding box that encapsulates the object
};

/******************************************************************************/
/*!
	Static Variables
*/
/******************************************************************************/

// list of original object
static GameObj				sGameObjList[GAME_OBJ_NUM_MAX];				// Each element in this array represents a unique game object (shape)
extern unsigned long		sGameDataNum;								// The number of defined game objects

// list of object instances
// Each element in this array represents a unique game object instance (sprite)
 extern GameObjInst			sGameObjInstList[GAME_OBJ_INST_NUM_MAX];
 extern GameObjDataInstance dGameObjDataList[GAME_OBJ_INST_NUM_MAX];
 extern unsigned long		sGameDataInstNum;							// The number of used game object instances


// mutex for the game object instance list

// pointer to the ship object
static GameObjInst* spShip;										// Pointer to the "Ship" game object instance


static std::map<uint32_t, GameObjDataInstance*> dataShips; // map of all the ships in the game
static std::mutex dataListMutex;
static std::mutex dataShipsMutex;

// pointer to the wall object
static GameObjInst* spWall;										// Pointer to the "Wall" game object instance

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Current score

// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst* gameObjInstCreate(unsigned long type, AEVec2* scale,
	AEVec2* pPos, AEVec2* pVel, float dir);
void				gameObjInstDestroy(GameObjInst* pInst);
// helper function for wall collision
void				Helper_Wall_Collision();
// random generator for number and for asteroid scale, position, velocity
void				Random_value_Generator(AEVec2& scale, AEVec2& pPos, AEVec2& pVel);

void				Random_number_asteroid_generator(int& number);


void GameStateAsteroidsLoad(void);
void GameStateAsteroidsInit(void);
void GameStateAsteroidsUpdate(void);
void GameStateAsteroidsDraw(void);
void GameStateAsteroidsFree(void);
void GameStateAsteroidsUnload(void);


namespace AsteroidGame
{
	void CreatePlayer(uint32_t playerID, AEVec2 pos, AEVec2 vel, AEVec2 scale, float dir);
}

// ---------------------------------------------------------------------------

#endif // CSD1130_GAME_STATE_PLAY_H_


