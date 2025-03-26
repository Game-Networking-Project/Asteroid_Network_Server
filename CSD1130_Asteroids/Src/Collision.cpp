/******************************************************************************/
/*!
\file		Collision.cpp
\author 	Cheong Jia Zen, jiazen.c, 2301549
\par    	jiazen.c@digipen.edu
\date   	February 06, 2024
\brief		This file contains the definition of function CollisionIntersection_RectRect()
			that will detect collision intersection using the static collision method and 
			dynamic-dynamic collision between two objects.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"

/**************************************************************************/
/*!
This function will detect the collision between two objects using aabb method.
The detection will use both static collision and dynamic - dynamic collision.
It will return true if it collide and false if it is not colliding. It will
also calculate the first time of collision and return it using reference.
	*/
/**************************************************************************/

bool CollisionIntersection_RectRect(const AABB & aabb1,          //Input
									const AEVec2 & vel1,         //Input 
									const AABB & aabb2,          //Input 
									const AEVec2 & vel2,         //Input
									float& firstTimeOfCollision) //Output: the calculated value of tFirst, below, must be returned here
{
	UNREFERENCED_PARAMETER(aabb1);
	UNREFERENCED_PARAMETER(vel1);
	UNREFERENCED_PARAMETER(aabb2);
	UNREFERENCED_PARAMETER(vel2);
	UNREFERENCED_PARAMETER(firstTimeOfCollision);

	// pseudocode...
	/*
	Implement the collision intersection over here.

	The steps are:	
	Step 1: Check for static collision detection between rectangles (static: before moving). 
				If the check returns no overlap, you continue with the dynamic collision test
					with the following next steps 2 to 5 (dynamic: with velocities).
				Otherwise you return collision is true, and you stop.
	
	Step 2: Initialize and calculate the new velocity of Vb
			tFirst = 0  //tFirst variable is commonly used for both the x-axis and y-axis
			tLast = dt  //tLast variable is commonly used for both the x-axis and y-axis

	Step 3: Working with one dimension (x-axis).
			if(Vb < 0)
				case 1
				case 4
			else if(Vb > 0)
				case 2
				case 3
			else //(Vb == 0)
				case 5

			case 6

	Step 4: Repeat step 3 on the y-axis

	Step 5: Return true: the rectangles intersect

	*/
	// implementation...
	float tLast = 0.0f; // initialise outside so it can be detected for the case 6 too
	// step1...
	if (aabb1.max.x < aabb2.min.x || aabb1.min.x > aabb2.max.x
		|| aabb1.max.y < aabb2.min.y || aabb1.min.y > aabb2.max.y) 
	{
		// step2...
		// assign initial value for both tFirst and tLast
		firstTimeOfCollision = 0;
		tLast = (float)AEFrameRateControllerGetFrameTime();
		// calculate the relative velocity
		AEVec2 Vrel = { 0,0 };
		Vrel.x = (vel2.x - vel1.x);
		Vrel.y = (vel2.y - vel1.y);
		//step3...
		// x_axis ---------------------------------------------------------------
		if (Vrel.x < 0)
		{
			// case 1
			if (aabb1.min.x > aabb2.max.x)
			{
				return 0;
			}
			// case 4 
			// case 4 revisited (1/2)
			else if (aabb1.max.x < aabb2.min.x)
			{
				if (firstTimeOfCollision < (aabb1.max.x - aabb2.min.x) / Vrel.x)
				{
					firstTimeOfCollision = (aabb1.max.x - aabb2.min.x) / Vrel.x;
				}
				else
				{
					firstTimeOfCollision = firstTimeOfCollision;
				}
			}
			// case 4 revisited (2/2)
			else if (aabb1.min.x < aabb2.max.x)
			{
				if (tLast < (aabb1.min.x - aabb2.max.x) / Vrel.x)
				{
					tLast = tLast;
				}
				else
				{
					tLast = (aabb1.min.x - aabb2.max.x) / Vrel.x;
				}
			}
		}
		else if (Vrel.x > 0)
		{
			// case 2
			// case 2 revisited (1/2)
			if (aabb1.min.x > aabb2.max.x)
			{
				if (firstTimeOfCollision < (aabb1.min.x - aabb2.max.x) / Vrel.x)
				{
					firstTimeOfCollision = (aabb1.min.x - aabb2.max.x) / Vrel.x;
				}
				else
				{
					firstTimeOfCollision = firstTimeOfCollision;
				}
				
			}
			// case 2 revisited (2/2)
			else if (aabb1.max.x > aabb2.min.x)
			{
				if (tLast < (aabb1.max.x - aabb2.min.x) / Vrel.x)
				{
					tLast = tLast;
				}
				else
				{
					tLast = (aabb1.max.x - aabb2.min.x) / Vrel.x;
				}
			}
			// case 3
			else if (aabb1.max.x < aabb2.min.x)
			{
				return 0;
			}
		}
		// case 5
		else
		{
			if (aabb1.max.x < aabb2.max.x)
			{
				return 0;
			}
			else if (aabb1.min.x > aabb2.max.x)
			{
				return 0;
			}
		}
		// case 6
		if (firstTimeOfCollision > tLast)
		{
			return 0;
		}

		// step4...
		// y_axis ---------------------------------------------------------------
		if (Vrel.y < 0)
		{
			// case 1
			if (aabb1.min.y > aabb2.max.y)
			{
				return 0;
			}

			// case 4 
			// case 4 revisited (1/2)
			else if (aabb1.max.y < aabb2.min.y)
			{
				if (firstTimeOfCollision < (aabb1.max.y - aabb2.min.y) / Vrel.y)
				{
					firstTimeOfCollision = (aabb1.max.y - aabb2.min.y) / Vrel.y;
				}
				else
				{
					firstTimeOfCollision = firstTimeOfCollision;
				}
			}
			// case 4 revisited (2/2)
			else if (aabb1.min.y < aabb2.max.y)
			{
				if (tLast < (aabb1.min.y - aabb2.max.y) / Vrel.y)
				{
					tLast = tLast;
				}
				else
				{
					tLast = (aabb1.min.y - aabb2.max.y) / Vrel.y;
				}
			}
		}
		else if (Vrel.y > 0)
		{
			// case 2
			// case 2 revisited (1/2)
			if (aabb1.min.y > aabb2.max.y)
			{
				if (firstTimeOfCollision < (aabb1.min.y - aabb2.max.y) / Vrel.y)
				{
					firstTimeOfCollision = (aabb1.min.y - aabb2.max.y) / Vrel.y;
				}
				else
				{
					firstTimeOfCollision = firstTimeOfCollision;
				}

			}
			// case 2 revisited (2/2)
			else if (aabb1.max.y > aabb2.min.y)
			{
				if (tLast < (aabb1.max.y - aabb2.min.y) / Vrel.y)
				{
					tLast = tLast;
				}
				else
				{
					tLast = (aabb1.max.y - aabb2.min.y) / Vrel.y;
				}
			}
			// case 3
			else if (aabb1.max.y < aabb2.min.y)
			{
				return 0;
			}
		}
		// case 5
		else
		{
			if (aabb1.max.y < aabb2.max.y)
			{
				return 0;
			}
			else if (aabb1.min.y > aabb2.max.y)
			{
				return 0;
			}
		}
		// case 6
		if (firstTimeOfCollision > tLast)
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		// step5...
		return 1; // later comment this out to check if the dynamic colllision work
	}
	return 0;
}