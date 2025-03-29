#include "AsteroidData.h"

#include <cstring>
#include <WinSock2.h>
void SetOwner(AsteroidData* data, uint8_t owner)
{
	data->owner = owner;
}

uint8_t GetOwner(AsteroidData* data)
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

void SetScale(AsteroidData* data, AEVec2 scale)
{
	data->scale = scale;
}

AEVec2 GetScale(AsteroidData* data)
{
	return data->scale;
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


void SetTime(AsteroidData* data, float time)
{
	data->time = time;
}

float GetTime(AsteroidData* data)
{
	return data->time;
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


void CopyVec2(void* dest, AEVec2& src, bool ToNetwork=true )
{
	float x = src.x;
	float y = src.y;
	if (ToNetwork)
	{
		u_long x1 = htonf(src.x);
		u_long y1 = htonf(src.y);

		memcpy(dest, &x1, sizeof(u_long));
		memcpy(reinterpret_cast<char*>(dest) + sizeof(u_long), &y1, sizeof(u_long));

	}
	else
	{
		memcpy(dest, &x, sizeof(float));
		memcpy(reinterpret_cast<char*>(dest) + sizeof(float), &y, sizeof(float));
	}
}

AEVec2 ExtractVec2(void* src, bool FromNetwork = true)
{
	AEVec2 vec;
	float x, y;
	if (FromNetwork)
	{
		u_long x1, y1;
		memcpy(&x1, src, sizeof(u_long));
		memcpy(&y1, reinterpret_cast<char*>(src) + sizeof(u_long), sizeof(u_long));
		x = ntohf(x1);
		y = ntohf(y1);
	}
	else
	{
		memcpy(&x, src, sizeof(float));
		memcpy(&y, reinterpret_cast<char*>(src) + sizeof(float), sizeof(float));
	}
	vec.x = x;
	vec.y = y;
	return vec;
}
char* ToNetworkData(AsteroidData* data)
{
	char* buffer = new char[DATA_SIZE] {};

	memcpy(buffer, &data->owner, sizeof(uint8_t));

	CopyVec2(buffer + 1, data->position);
	CopyVec2(buffer + 9, data->scale);
	CopyVec2(buffer + 17, data->velocity);
	CopyVec2(buffer + 25, data->direction);
	u_long scoreCount = htonl(data->scoreCount);
	memcpy(buffer + 33, &scoreCount, sizeof(u_long));
	u_long n_time = htonf(data->time);
	memcpy(buffer + 37, &n_time, sizeof(u_long));


	return  buffer;


}

AsteroidData FromNetworkData(char* buffer)
{
	AsteroidData data;
	data.owner = *buffer;
	data.position = ExtractVec2(buffer + 1);
	data.scale = ExtractVec2(buffer + 9);
	data.velocity = ExtractVec2(buffer + 17);
	data.direction = ExtractVec2(buffer + 25);
	u_long scoreCount;
	memcpy(&scoreCount, buffer + 33, sizeof(u_long));
	data.scoreCount = ntohl(scoreCount);
	u_long n_time;
	memcpy(&n_time, buffer + 37, sizeof(u_long));
	data.time = ntohf(n_time);
	return data;
}


