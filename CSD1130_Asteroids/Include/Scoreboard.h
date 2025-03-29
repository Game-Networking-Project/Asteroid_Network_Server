#pragma once

#include <map>

int create_scoreboard_buffer(uint8_t* buffer, size_t bufferSize, uint8_t cmdID, Player* players, int playerCount);