#ifndef ASTEROIDDATA_H
#define ASTEROIDDATA_H
#include <AEVec2.h>

struct AsteroidData
{
	int owner;
	AEVec2 position;
	AEVec2 velocity;
	AEVec2 direction;
	int scoreCount;

	
};

void SetOwner(AsteroidData* data, int owner);
int GetOwner(AsteroidData* data);

void SetPosition(AsteroidData* data, AEVec2 position);
AEVec2 GetPosition(AsteroidData* data);

void SetVelocity(AsteroidData* data, AEVec2 velocity);
AEVec2 GetVelocity(AsteroidData* data);

void SetDirection(AsteroidData* data, AEVec2 direction);
AEVec2 GetDirection(AsteroidData* data);

void SetScoreCount(AsteroidData* data, int scoreCount);
int GetScoreCount(AsteroidData* data);

void CalculateLinearConvergent(::AsteroidData* data, float time);



#endif