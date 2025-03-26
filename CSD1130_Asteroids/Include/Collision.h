/******************************************************************************/
/*!
\file		Collision.h
\author 	Cheong Jia Zen, jiazen.c, 2301549
\par    	jiazen.c@digipen.edu
\date   	February 06, 2024
\brief		This file contains the declaration of function CollisionIntersection_RectRect()
			that will detect collision intersection using the static collision method and
			dynamic-dynamic collision between two objects.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#ifndef CSD1130_COLLISION_H_
#define CSD1130_COLLISION_H_

#include "AEEngine.h"

/**************************************************************************/
/*!

	*/
/**************************************************************************/
struct AABB
{
	AEVec2	min;
	AEVec2	max;
};

bool CollisionIntersection_RectRect(const AABB& aabb1,            //Input
									const AEVec2& vel1,           //Input 
									const AABB& aabb2,            //Input 
									const AEVec2& vel2,           //Input
									float& firstTimeOfCollision); //Output: the calculated value of tFirst, must be returned here


#endif // CSD1130_COLLISION_H_