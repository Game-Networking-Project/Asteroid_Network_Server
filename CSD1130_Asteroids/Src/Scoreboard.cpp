#include "Scoreboard.h"

// Player Structure
// Identified by a player id
// With their respective score
struct Player {
	int player_id;
	int score = 0;
};

// List of players
std::map<int, Player> list_of_players;