#include  "AsteroidData.h"; 
export module ServerState;
import <cstdint>;
import <vector>;
import <mutex>;
// Export constants
export constexpr int MAX_IP_ADDRESS_LEN_STR = 256;
export constexpr int MAX_PORT_LEN_STR = 16;

export struct ClientPlayer
{
	char Client_Address[MAX_IP_ADDRESS_LEN_STR];
	char Client_Port[MAX_PORT_LEN_STR];

	uint32_t IP_Address;
	uint16_t Port;


};



export struct WorldState
{
	std::atomic<size_t> numPlayers;
	std::atomic<size_t> numAsteroids;
	std::atomic<size_t> numBullets;



	std::vector<ClientPlayer> Players;
	std::vector<AsteroidData> Asteroids;


	// Mutexes for the lists
	std::mutex PlayerList;
	std::mutex AsteroidList;


	std::condition_variable PlayerCount;
	std::condition_variable AsteroidCount;
	std::condition_variable BulletCount;

	// Mutex for the condition variables
	std::mutex PlayerCountMutex;
	std::mutex AsteroidCountMutex;
	std::mutex BulletCountMutex;

};

export struct ServerState
{
	char IP_Address[MAX_IP_ADDRESS_LEN_STR];
	char Port[MAX_PORT_LEN_STR];

	uint32_t ServerIP;
	uint16_t ServerSocket;

	std::mutex WorldStateMutex; // Mutex to lock the world state , not sure if this is needed
	WorldState world;



	
};



