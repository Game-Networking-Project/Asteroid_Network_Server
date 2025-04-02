#include "EngineUtility.h"
#include "GameState_Asteroids.h"

import ServerState;


GameObjDataInstance* gameDataInstCreate(unsigned long type, AEVec2* scale,
	AEVec2* pPos, AEVec2* pVel, float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero);

	AE_ASSERT_PARM(type < sGameDataNum);


	std::lock_guard<std::mutex> lock(dataListMutex);

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjDataInstance* pInst = dGameObjDataList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
			pInst->objectType = static_cast<TYPE>(type);
			pInst->flag = FLAG_ACTIVE;
			pInst->scale = *scale;
			pInst->posCurr = pPos ? *pPos : zero;
			pInst->velCurr = pVel ? *pVel : zero;
			pInst->dirCurr = dir;

			// return the newly created instance

			if (type == static_cast<TYPE>(TYPE_ASTEROID))
			{
				pInst->positionInServerDB = Server::GetNumAsteroids(&server);

			}
			else if (type == static_cast<TYPE>(TYPE_SHIP))
			{
				pInst->positionInServerDB = Server::GetNumPlayers(&server);
			}
			else if (type == static_cast<TYPE>(TYPE_BULLET))
			{
				pInst->positionInServerDB = Server::GetNumBullets(&server);
			}


			return pInst;
		}
	}

	

	// cannot find empty slot => return 0
	return 0;
}


void gameDataInstDestroy(GameObjDataInstance* pInst)
{
	// check if the instance is valid
	if (pInst)
	{
		if (pInst->objectType == TYPE_ASTEROID)
		{
			Server::RemoveAsteroid(&server, pInst->positionInServerDB);
		}
		else if (pInst->objectType == TYPE_SHIP)
		{
			//Server::RemovePlayer(&server, pInst->positionInServerDB);
		}
		else if (pInst->objectType == TYPE_BULLET)
		{
			Server::RemoveBullet(&server, pInst->positionInServerDB);
		}
		
		// delete the instance
		delete pInst;

		
	}
}
