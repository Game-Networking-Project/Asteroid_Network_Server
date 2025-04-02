#ifndef SHIPSTATE_H
#include <AEVec2.h>
// ships data
struct ShipsState
{
	bool active;
	AEVec2 Position;
	AEVec2 Velocity;
	float CurrentDirection; // for rotation
	uint32_t lastFiredTime; // time stamp on when 
	bool isFiring;
};

// bullets data
struct BulletsState
{
	uint32_t bulletID;         // Unique identifier for the bullet
	AEVec2 Position;
	AEVec2 Velocity;
	float CurrentDirection; // for rotation
	uint32_t spawnTime;       // Timestamp of when the bullet was fired
};

// which player am I, and my state
struct ShipStateUpdateMessage {

	uint32_t playerID;        // Unique identifier for the player
	ShipsState state;          // Current state of the ship
};

// who and when client fired the bullet
struct FireEventMessage {
	uint32_t playerID;        // Player who fired
	uint32_t timestamp;       // Timestamp of when the firing occurred
};

// i am the bullet of who, and whats my state
struct BulletStateUpdateMessage {
	bool active;
	uint32_t playerID;
	BulletsState bulletState;  // Bullet state (position, velocity, etc.)
};
#endif