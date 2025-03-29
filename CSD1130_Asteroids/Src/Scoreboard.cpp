#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <iostream>
#include "Scoreboard.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

// Packet types/command IDs
#define CMD_SCOREBOARD_UPDATE 0x01


// Player Structure
// Identified by a player id
// With their respective score
struct Player {
	int player_id;
	int score = 0;
};

// List of players
std::map<int, Player> list_of_players;

// Scoreboard packet
typedef struct {
	uint8_t cmd_id;
	uint16_t player_count;
} ScoreboardHeader;

typedef struct {
	int32_t player_id;
	int32_t score;
} ScoreboardPlayerEntry;

int create_scoreboard_buffer(uint8_t* buffer, size_t bufferSize, uint8_t cmdID, Player* players, int playerCount) {
	int offset = 0;
	size_t required_size = sizeof(ScoreboardHeader) + (playerCount * sizeof(ScoreboardPlayerEntry));
	if (bufferSize < required_size) {
		return -1;
	}

	// Write command id
	buffer[offset++] = CMD_SCOREBOARD_UPDATE;

	// Write player count in network byte order
	uint16_t netPlayerCount = htons((uint16_t)playerCount);
	memcpy(buffer + offset, &netPlayerCount, sizeof(uint16_t));
	offset += sizeof(uint16_t);

	// Write each player's data in network byte order
	for (int i = 0; i < playerCount; i++) {
		// Write player ID
		int32_t net_playerID = htonl(players[i].player_id);
		memcpy(buffer + offset, &net_playerID, sizeof(int32_t));
		offset += sizeof(int32_t);

		// Write player score
		int32_t net_score = htonl(players[i].score);
		memcpy(buffer + offset, &net_score, sizeof(int32_t));
		offset += sizeof(int32_t);
	}

	return offset;
}