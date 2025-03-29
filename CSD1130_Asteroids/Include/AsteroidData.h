#ifndef ASTEROIDDATA_H
#define ASTEROIDDATA_H
#include <AEVec2.h>
#define DATA_SIZE 41
struct AsteroidData
{
	uint8_t owner;
	AEVec2 position;
	AEVec2 scale;
	AEVec2 velocity;
	AEVec2 direction;
	int scoreCount;
	float time;


	
};

void SetOwner(AsteroidData* data, uint8_t owner);
uint8_t GetOwner(AsteroidData* data);

void SetPosition(AsteroidData* data, AEVec2 position);
AEVec2 GetPosition(AsteroidData* data);

void SetScale(AsteroidData* data, AEVec2 scale);
AEVec2 GetScale(AsteroidData* data);

void SetVelocity(AsteroidData* data, AEVec2 velocity);
AEVec2 GetVelocity(AsteroidData* data);

void SetDirection(AsteroidData* data, AEVec2 direction);
AEVec2 GetDirection(AsteroidData* data);

void SetScoreCount(AsteroidData* data, int scoreCount);
int GetScoreCount(AsteroidData* data);

void SetTime(AsteroidData* data, float time);
float GetTime(AsteroidData* data);

void CalculateLinearConvergent(::AsteroidData* data, float time);
char* ToNetworkData(AsteroidData* data);

AsteroidData FromNetworkData(char* buffer);



#endif