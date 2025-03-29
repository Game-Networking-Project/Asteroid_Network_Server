#include "AsteroidData.h"

void SetOwner(AsteroidData* data, int owner)
{
	data->owner = owner;
}

int GetOwner(AsteroidData* data)
{
	return data->owner;
}

void SetPosition(AsteroidData* data, AEVec2 position)
{
	data->position = position;
}

AEVec2 GetPosition(AsteroidData* data)
{
	return data->position;
}

void SetVelocity(AsteroidData* data, AEVec2 velocity)
{
	data->velocity = velocity;
}

AEVec2 GetVelocity(AsteroidData* data)
{
	return data->velocity;
}

void SetDirection(AsteroidData* data, AEVec2 direction)
{
	data->direction = direction;
}

AEVec2 GetDirection(AsteroidData* data)
{
	return data->direction;
}

void SetScoreCount(AsteroidData* data, int scoreCount)
{
	data->scoreCount = scoreCount;
}

int GetScoreCount(AsteroidData* data)
{
	return data->scoreCount;
}


void CalculateLinearConvergent(AsteroidData* data, float time)
{
	AEVec2 position = GetPosition(data);
	AEVec2 velocity = GetVelocity(data);
	AEVec2 direction = GetDirection(data);
	position.x += velocity.x * time;
	position.y += velocity.y * time;
	SetPosition(data, position);
}


