module;
#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include  "AsteroidData.h"; 
#include "GameState_Asteroids.h"
#include "ShipState.h"
#include "udpf.h"
export module ServerState;
import <cstdint>;
import <vector>;
import <mutex>;
import <condition_variable>;

constexpr int TEST_PLAYER = 4;
constexpr int TEST_ASTEROID = 4;
constexpr int TEST_BULLET = 50;

constexpr int TEST_PLAYER_CONNECTION_DELAY = 100;
constexpr int TEST_ASTEROID_SPAWN_DELAY = 1000;
constexpr int TEST_BULLET_SPAWN_DELAY = 100;


// Export constants
export constexpr int MAX_IP_ADDRESS_LEN_STR = 256;
export constexpr int MAX_PORT_LEN_STR = 16;

export constexpr bool FAKE_TEST = true;

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
	std::vector<BulletStateUpdateMessage> Bullets;


	// Mutexes for the lists
	std::mutex PlayerList;
	std::mutex AsteroidList;
	std::mutex BulletList;


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


	void HandleInput(); // Server command input


	void Listen(uint16_t port,  bool TEST=false);
	
};


export namespace Server
{
	void CreateAsteroid(ServerState* _self, AEVec2 pos, AEVec2 vel, AEVec2 scale, float dir); // Create an asteroid
	void CreateBullet(ServerState* _self, uint32_t owner, AEVec2 pos, AEVec2 vel, AEVec2 scale, float dir); // Create a bullet
	void CreatePlayer(ServerState* _self , SOCKET clientSocket); // Create a player

	void KickPlayer(ServerState* _self, SOCKET clientSocket); // Remove a player
	void RemoveAsteroid(ServerState* _self, size_t pos); // Remove an asteroid
	void RemoveBullet(ServerState* _self, size_t pos); // Remove a bullet


	size_t GetNumPlayers(ServerState* state);
	size_t GetNumAsteroids(ServerState* state);
	size_t GetNumBullets(ServerState* state);



	void DebugPrintAsteroids(ServerState* state);
}



void Server::CreateAsteroid(ServerState* _self, AEVec2 pos, AEVec2 vel, AEVec2 scale, float dir)
{
	std::lock_guard<std::mutex> asteroidListLock{_self->world.AsteroidList};
	

	AsteroidData asteroid;
	SetPosition(&asteroid, pos);
	SetVelocity(&asteroid, vel);
	SetScale(&asteroid, scale);
	SetDirection(&asteroid, { cosf(dir), sinf(dir) });
	SetTime(&asteroid, 0.0f);
	_self->world.Asteroids.push_back(asteroid); // Add the asteroid to the list
	++_self->world.numAsteroids; // Increment the number of asteroids
	_self->world.AsteroidCount.notify_all(); // Notify all threads that an asteroid has been created
	


}

void Server::CreateBullet(ServerState* _self, uint32_t owner, AEVec2 pos, AEVec2 vel, AEVec2 scale, float dir)
{
	std::lock_guard<std::mutex> bulletListLock{ _self->world.BulletList };
	BulletStateUpdateMessage bullet;
	bullet.playerID = owner;
	bullet.bulletState.Position = pos;
	bullet.bulletState.Velocity = vel;
	bullet.bulletState.CurrentDirection = dir;
	bullet.bulletState.spawnTime = GetTickCount();
	_self->world.Bullets.push_back(bullet); // Add the bullet to the list
	++_self->world.numBullets; // Increment the number of bullets
	_self->world.BulletCount.notify_all(); // Notify all threads that a bullet has been created
}

void Server::CreatePlayer(ServerState* _self, SOCKET clientSocket)
{
	std::lock_guard<std::mutex> playerListLock{ _self->world.PlayerList };
	ClientPlayer player;

	// Get client address
	sockaddr_in clientAddr;
	int addrLen = sizeof(clientAddr);
	getpeername(clientSocket, (sockaddr*)&clientAddr, &addrLen);
	inet_ntop(AF_INET, &clientAddr.sin_addr, player.Client_Address, MAX_IP_ADDRESS_LEN_STR);
	player.IP_Address = ntohl(clientAddr.sin_addr.S_un.S_addr);
	player.Port = ntohs(clientAddr.sin_port);

	_self->world.Players.push_back(player); // Add the player to the list
	++_self->world.numPlayers; // Increment the number of players
	_self->world.PlayerCount.notify_all();
	
}

void Server::KickPlayer(ServerState* _self, SOCKET clientSocket)
{
	std::lock_guard<std::mutex> playerListLock{ _self->world.PlayerList };
	ClientPlayer player;
	// Get client address
	sockaddr_in clientAddr;
	int addrLen = sizeof(clientAddr);
	getpeername(clientSocket, (sockaddr*)&clientAddr, &addrLen);
	inet_ntop(AF_INET, &clientAddr.sin_addr, player.Client_Address, MAX_IP_ADDRESS_LEN_STR);
	player.IP_Address = ntohl(clientAddr.sin_addr.S_un.S_addr);
	player.Port = ntohs(clientAddr.sin_port);
	// Find the player in the list
	auto it = std::find_if(_self->world.Players.begin(), _self->world.Players.end(), [&player](const ClientPlayer& p) { return p.IP_Address == player.IP_Address && p.Port == player.Port; });
	if (it != _self->world.Players.end())
	{
		_self->world.Players.erase(it); // Remove the player from the list
	}
}


void Server::RemoveAsteroid(ServerState* _self, size_t pos)
{
	std::lock_guard<std::mutex> asteroidListLock{ _self->world.AsteroidList };
	_self->world.Asteroids.erase(_self->world.Asteroids.begin() + pos); // Remove the asteroid from the list
	--_self->world.numAsteroids; // Decrement the number of asteroids
	_self->world.AsteroidCount.notify_all(); // Notify all threads that an asteroid has been removed
}

void Server::RemoveBullet(ServerState* _self, size_t pos)
{
	std::lock_guard<std::mutex> bulletListLock{ _self->world.BulletList };
	_self->world.Bullets.erase(_self->world.Bullets.begin() + pos); // Remove the bullet from the list
	--_self->world.numBullets; // Decrement the number of bullets
	_self->world.BulletCount.notify_all(); // Notify all threads that a bullet has been removed
}



void ServerState::HandleInput()
{
	//drop first input
	std::string input;
	std::getline(std::cin, input);

	bool isExit = false;

	while (!isExit)
	{
		std::string input;
		std::cout << "> ";
		std::getline(std::cin, input);
		if (input == "exit")
		{
			isExit = true;
		}
		else if (input == "ping")
		{
			std::cout << "Pong" << std::endl;
		}
		else if (input == "GetPlayers()")
		{
			std::cout << "Number of players: " << Server::GetNumPlayers(this) << std::endl;
		}
		else if (input == "GetAsteroids()")	
		{
			std::cout << "Number of asteroids: " << Server::GetNumAsteroids(this) << std::endl;
		}
		else if (input == "GetBullets()")
		{

			std::cout << "Number of bullets: " << Server::GetNumBullets(this) << std::endl;
		}
		else if (input == "PrintAsteroids()")
		{
			Server::DebugPrintAsteroids(this);
		}
		else
		{
			std::cout << "Invalid command" << std::endl;
		}
	}

	std::cout << "CLI Exiting" << std::endl;
}


size_t Server::GetNumAsteroids(ServerState* state)
{
	std::lock_guard<std::mutex> asteroidListLock{ state->world.AsteroidCountMutex };
	return state->world.numAsteroids;

}

size_t Server::GetNumBullets(ServerState* state)
{
	std::lock_guard<std::mutex> bulletListLock{ state->world.BulletCountMutex };
	return state->world.numBullets;
}

size_t Server::GetNumPlayers(ServerState* state)
{
	std::lock_guard<std::mutex> playerListLock{ state->world.PlayerCountMutex };
	return state->world.numPlayers;
}

void Server::DebugPrintAsteroids(ServerState* state)
{
	std::cout << "Asteroids: " << std::endl;
	std::cout << "--------------------------------" << std::endl;
	std::cout << "ID\tPosition\tVelocity\tScale\tDirection" << std::endl;
	std::cout << "--------------------------------" << std::endl;
	// Print the asteroids
	int i = 0;
	std::lock_guard<std::mutex> asteroidListLock{ state->world.AsteroidList };
	for (const auto& asteroid : state->world.Asteroids)
	{
		std::cout << i << "\t";
		std::cout << asteroid.position.x << ", " << asteroid.position.y << "\t";
		std::cout << asteroid.velocity.x << ", " << asteroid.velocity.y << "\t";
		std::cout << asteroid.scale.x << ", " << asteroid.scale.y << "\t";
		std::cout << asteroid.direction.x << ", " << asteroid.direction.y << std::endl;
		++i;
	}
	std::cout << "--------------------------------" << std::endl;
}




void ServerState::Listen(uint16_t port, bool TEST)
{
	
	udpf_interface::endpoint_addr_in localAddr_in{ udpf_interface::endpoint_addr_info(port) };
	udpf_interface::listener l{ localAddr_in };

	udpf_interface::connection_config cconfig;										//connection configurations
	cconfig.connection_timeout_mili = 30000;						//30sec
	cconfig.max_datagram_size = 400;
	cconfig.max_window_size = 16;
	cconfig.packet_timeout_mili = 150;

	std::cout << "Server IP Address: " << l.local_addr.get_ip_string() << std::endl;
	std::cout << "Server Port Number: " << l .local_addr.get_port_string() << std::endl;





	if (TEST) std::cout << "Server is running in test mode..." << std::endl;
	else std::cout << "Server is running in production mode..." << std::endl;
	while (true)
	{
		if (TEST)
		{
			//Spawn player every 100ms
			for (int i = 0; i < TEST_PLAYER; i++)
			{
				Server::CreatePlayer(this, {});

				AsteroidGame::CreatePlayer(i, { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f);


				std::this_thread::sleep_for(std::chrono::milliseconds(TEST_PLAYER_CONNECTION_DELAY));
			}
		}
		else
		{
			auto conn_opt = l.listen();
		}
	}
}



export ServerState server{};