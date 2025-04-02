#ifndef EngineUtility_H
#define EngineUtility_H
#include <AEVec2.h>


constexpr unsigned int	GAME_OBJ_NUM_MAX = 32;			// The total number of different objects (Shapes)
constexpr unsigned int	GAME_OBJ_INST_NUM_MAX = 2048;

constexpr unsigned int DATA_TYPE_COUNT_MAX = 32; // The total number of different data types
constexpr unsigned int DATA_OBJ_INST_NUM_MAX = 2048; // The total number of different data instances


struct GameObjDataInstance;

struct GameObjInst;


GameObjDataInstance* gameDataInstCreate(unsigned long type, AEVec2* scale,
	AEVec2* pPos, AEVec2* pVel, float dir);

void				gameDataInstDestroy(GameObjDataInstance* pInst);


#endif