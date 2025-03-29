/******************************************************************************/
/*!
\file		Main.cpp
\author 	Cheong Jia Zen, jiazen.c, 2301549
\par    	jiazen.c@digipen.edu
\date   	February 06, 2024
\brief		This file contains the function that is the driver for
			executing this program.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#define MAX_IP_ADDRESS_LEN_STR 256
#define MAX_PORT_LEN_STR 16

#include <iostream>

#include "main.h"

#include <memory>


// C++ 20 module
import ServerState;
import <mutex>;

// Global variable
ServerState serverState{};

// ---------------------------------------------------------------------------
// Globals
float	 g_dt;
double	 g_appTime;

#pragma comment(lib, "ws2_32.lib")

char SERVER_PORT_BUFFER[MAX_PORT_LEN_STR];




constexpr short PORT = 0;


/******************************************************************************/
/*!
	Starting point of the application
*/
/******************************************************************************/
/******************************************************************************/
/*!
	Main function for the game loop, driver for executing this program (game loop)
*/
/******************************************************************************/
int WINAPI WinMain(HINSTANCE instanceH, HINSTANCE prevInstanceH, LPSTR command_line, int show)
{
	// Create a console window
	AllocConsole();
	// Attach the console window to this application's process
	AttachConsole(GetCurrentProcessId());
	// Redirect the CRT standard input, output, and error handles to the console window
	freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);






	UNREFERENCED_PARAMETER(prevInstanceH);
	UNREFERENCED_PARAMETER(command_line);

	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	//int * pi = new int;
	////delete pi;


	// Initialize the system
	AESysInit(instanceH, 0, 800, 600, 0, 60, false, NULL);


	// Initialize winsock
	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	// Check if the initialization is successful
	if (WSAStartup(ver, &wsData) != 0)
	{
		std::cerr << "Error: Failed to initialize winsock" << std::endl;
		return 1;
	}




	std::cout << "Enter the port number: ";
	std::cin >> SERVER_PORT_BUFFER;

	// Get local IP address
	char hostname[256];
	gethostname(hostname, sizeof(hostname));
	struct hostent* host = gethostbyname(hostname);

	// Check if the host is valid
	if (host == nullptr)
	{
		std::cerr << "Error: Failed to get host" << std::endl;
		return 1;
	}

	// Get the IP address
	char* ip = inet_ntoa(*(struct in_addr*)*host->h_addr_list);

	std::cout << "IP Address: " << ip << std::endl;
	std::cout << "Port: " << SERVER_PORT_BUFFER << std::endl;








	GameStateMgrInit(GS_ASTEROIDS);

	std::cout << "Waiting for Player to connect..." << std::endl;
	std::unique_lock<std::mutex> lock(serverState.world.PlayerCountMutex);
	serverState.world.PlayerCount.wait(lock, [&] {return serverState.world.numPlayers > 0; });
	std::cout << "Player connected!" << std::endl;
	while (gGameStateCurr != GS_QUIT)
	{

		// reset the system modules
		AESysReset();

		// If not restarting, load the gamestate
		if (gGameStateCurr != GS_RESTART)
		{
			GameStateMgrUpdate();
			GameStateLoad();
		}
		else
			gGameStateNext = gGameStateCurr = gGameStatePrev;

		// Initialize the gamestate
		GameStateInit();

		while (gGameStateCurr == gGameStateNext)
		{
			AESysFrameStart();

			GameStateUpdate();

			GameStateDraw();

			AESysFrameEnd();

			// check if forcing the application to quit
			if ((AESysDoesWindowExist() == false) || AEInputCheckTriggered(AEVK_ESCAPE))
				gGameStateNext = GS_QUIT;

			g_dt = (f32)AEFrameRateControllerGetFrameTime();
			g_appTime += g_dt;
		}

		GameStateFree();

		if (gGameStateNext != GS_RESTART)
			GameStateUnload();

		gGameStatePrev = gGameStateCurr;
		gGameStateCurr = gGameStateNext;
	}

	// free the system
	AESysExit();
	FreeConsole();
}