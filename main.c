#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "amcom.h"
#include "amcom_packets.h"
#include "math.h"

#define DEFAULT_TCP_PORT 	"2001"

float calculateDistance(float x_food, float y_food, float x_player, float y_player)
{
	static float dist;
	dist = ((x_player-x_food)*(x_player-x_food)) + ((y_player-y_food)*(y_player-y_food));
	dist =  sqrt(dist);
	return dist;
}

float calculateAngle(float x_food, float y_food, float x_player, float y_player)
{
	static float ang;
	ang = atan2(y_food - y_player, x_food - x_player);
	if (ang < 0)
		ang += 6.28f;
	return ang;
}

float intercept (float playerX, float playerY, float opponentX, float opponentY, float opponentMoveAngle)
{
	float middleX = (playerX + opponentX) / 2;
	float middleY = (playerY + opponentY) / 2;

	float a1 = tan(calculateAngle(middleX, middleY, playerX, playerY));
	float a2 = -1 / a1;							// perpendicular line equation 

	float b2 = middleY - a2*middleX;			// the equation of the line passing through the middle point (y = a2*x + b)
	float b3 = opponentY - opponentMoveAngle*opponentX;

	// http://www.math.edu.pl/punkt-przeciecia-dwoch-prostych
	// calculating point of interception
	float W = a2 - opponentMoveAngle;				// determinant (since B factors are equal to 1)
	float Wx = b3 - b2;
	float Wy = opponentMoveAngle*b2 - b3*a2;

	float pointOfInterceptionX = Wx/W;
	float pointOfInterceptioY  = Wy/W;

	return calculateAngle(playerX, playerY, pointOfInterceptionX, pointOfInterceptioY);
}

/*
*	Example usage:
*	float *intercept = interceptPoint(300, 300, 400, 400, calculateAngle(300, 300, 400, 400));
*   printf("X:   %f\nY:    %f", *intercept, *(intercept + 1));
*
*	@param returns pointer to array of X, Y of intercept point
*/

float * interceptPoint (float playerX, float playerY, float opponentX, float opponentY, float opponentMoveAngle)
{
	float middleX = (playerX + opponentX) / 2;
	float middleY = (playerY + opponentY) / 2;

	float a1 = tan(calculateAngle(middleX, middleY, playerX, playerY));	// a = tan(alpha) -> slope of linear function
	
	float A1 = -1 / a1;					// perpendicular line equation 
    	float A2 = opponentMoveAngle;

	float C1 = middleY - A1*middleX;			// the equation of the line passing through the middle point (y = a2*x + b)
	float C2 = opponentY - A2*opponentX;

	// http://www.math.edu.pl/punkt-przeciecia-dwoch-prostych
	// calculating point of interception
	float W = A1 - A2;					// determinant (since B factors are equal to 1)
	float Wx = C2 - C1;
	float Wy = C1*A2 - A1*C2;

	static float pointOfInterception[2];

	pointOfInterception[0] = Wx/W;
	pointOfInterception[1]  = Wy/W;

	return pointOfInterception;
}

void optimalFood (AMCOM_FoodUpdateRequestPayload *foodUpdateRequestPayload, AMCOM_MoveRequestPayload *moveRequestPayload, float *posX, float *posY, bool *isAnyFoodLeft)
{
	// check distances to food
	float foodDistanceToFood, tempDistance, sumOfDistances, lowestSumOfDistances = 999999999999.0;

	for (int i = 0; i < AMCOM_MAX_FOOD_UPDATES; ++i)
	{
		if (foodUpdateRequestPayload->foodState[i].state == 1)
		{
			tempDistance = calculateDistance(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, 
												moveRequestPayload->x, moveRequestPayload->y);

			// Depth 2
			// calculation of distances between food
			for (int j = 0; j < AMCOM_MAX_FOOD_UPDATES; ++j)
			{
				if (foodUpdateRequestPayload->foodState[j].state == 1)
				{
					foodDistanceToFood = calculateDistance(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, 
														foodUpdateRequestPayload->foodState[j].x,foodUpdateRequestPayload->foodState[j].y);
					sumOfDistances = foodDistanceToFood + tempDistance;

					if (sumOfDistances <= lowestSumOfDistances)
					{
						lowestSumOfDistances = sumOfDistances;
						float distanceToSecondFood = calculateDistance(foodUpdateRequestPayload->foodState[j].x, foodUpdateRequestPayload->foodState[j].y, 
												moveRequestPayload->x, moveRequestPayload->y);

						if (tempDistance <= distanceToSecondFood)
						{
							*posX = foodUpdateRequestPayload->foodState[i].x;
							*posY =  foodUpdateRequestPayload->foodState[i].y;
							*isAnyFoodLeft = true;
						}
						else
						{
							*posX = foodUpdateRequestPayload->foodState[j].x;
							*posY =  foodUpdateRequestPayload->foodState[j].y;
							*isAnyFoodLeft = true;
						}
					}
				}
			}
		}
	}
}

/*
* 	This function has to consider, whether chasing player is a danger for us, should we attack, or 
*	rather be interested in eating food.
*	
*	@param returns angle in radians
*/

float moveStrengthEvaluation (AMCOM_PlayerUpdateRequestPayload *playerUpdateRequestPayload, AMCOM_MoveRequestPayload *moveRequestPayload, 
							float *posX, float *posY, bool isAnyFoodLeft)
{
	// find the closest player
	float rivalPosX 				= 99999999.0;
	float rivalPosY 				= 99999999.0;
	float closestRivalDistance 		= 999999.0;
	float tempRivalDistance;
	uint16_t closestRivalHp 		= 0;
	uint8_t closestRivalID;
	static float playerPreviousPositionX[AMCOM_MAX_PLAYER_UPDATES];
	static float playerPreviousPositionY[AMCOM_MAX_PLAYER_UPDATES];

	for (int i = 0; i < AMCOM_MAX_PLAYER_UPDATES; ++i)
	{
		tempRivalDistance = calculateDistance(playerUpdateRequestPayload->playerState[i].x, playerUpdateRequestPayload->playerState[i].y, 
								moveRequestPayload->x, moveRequestPayload->y);
		if (tempRivalDistance <= closestRivalDistance)
		{
			closestRivalDistance = tempRivalDistance;
			rivalPosX = playerUpdateRequestPayload->playerState[i].x;
			rivalPosY = playerUpdateRequestPayload->playerState[i].y;
			closestRivalHp = playerUpdateRequestPayload->playerState[i].hp;
			closestRivalID = i;
		}
	}

	for (int i = 0; i < AMCOM_MAX_PLAYER_UPDATES; ++i)
	{
		playerPreviousPositionX[i] = rivalPosX;
		playerPreviousPositionY[i] = rivalPosY;
	}

	float closestRivalMoveAngle = calculateAngle(playerPreviousPositionX[closestRivalID], playerPreviousPositionY[closestRivalID], rivalPosX, rivalPosY);
	float toOurPlayerAngle		= calculateAngle(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY);

	// run away from him
	if (closestRivalHp > playerUpdateRequestPayload->playerState->hp)
	{
		// if toOurPlayerAngle is between +- 5 degree the direction of enemy player movement
		if (toOurPlayerAngle < closestRivalMoveAngle + 0.09 && toOurPlayerAngle > closestRivalMoveAngle - 0.09)
		{
			if (isAnyFoodLeft)
			{
				// calculate optimal course if able, we want here to catch and 
				return calculateAngle(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);	
			}

			else
				return calculateAngle(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY);
		}
		else
		{
			return calculateAngle(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);
		}
	}
	
	else if (closestRivalHp < playerUpdateRequestPayload->playerState->hp)
	{
		// evalutaion of 
		if (isAnyFoodLeft)
		{
			// distance to closest food
			// distance to closest player

			// TODO: next line should be deleted!!!
			return intercept(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY, closestRivalMoveAngle);
		}
		else
			return intercept(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY, closestRivalMoveAngle);

	}

	else
		return calculateAngle(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);
}

/**
 * This function will be called each time a valid AMCOM packet is received
 */
void amcomPacketHandler(const AMCOM_Packet* packet, void* userContext) {
	uint8_t amcomBuf[AMCOM_MAX_PACKET_SIZE];	// buffer used to serialize outgoing packets
	size_t bytesToSend = 0;						// size of the outgoing packet
	static int playerCounter;					// just a counter to distinguish player instances
	SOCKET sock = (SOCKET)userContext;			// socket used for communication with the game client

	// written by me
	static AMCOM_NewGameRequestPayload newGameRequestPayload;		
	static AMCOM_MoveResponsePayload moveResponsePayload;			
	static AMCOM_PlayerUpdateRequestPayload playerUpdateRequestPayload;	
	static AMCOM_FoodUpdateRequestPayload foodUpdateRequestPayload;	
	static AMCOM_FoodUpdateRequestPayload tempFood;
	static AMCOM_MoveRequestPayload moveRequestPayload;

	static bool isFirstCall = true;
	static float tempDistance;
	static float lowDistance;
	static float posX;
	static float posY;
	static bool isAnyFoodLeft = false;
	static float rivalPosX;
	static float rivalPosY;
	static float tempRivalDistance;
	static float closestRivalDistance;
	static uint16_t closestRivalHp;

	switch (packet->header.type) {
	case AMCOM_IDENTIFY_REQUEST:
		printf("Got IDENTIFY.request. Responding with IDENTIFY.response\n");
		AMCOM_IdentifyResponsePayload identifyResponse;
		sprintf(identifyResponse.playerName, "krzysiu%d", playerCounter++);
		bytesToSend = AMCOM_Serialize(AMCOM_IDENTIFY_RESPONSE, &identifyResponse, sizeof(identifyResponse), amcomBuf);
		break;
	case AMCOM_NEW_GAME_REQUEST:
		printf("Got NEW_GAME.request.\n");
		// TODO: respond with NEW_GAME.confirmation

		// akceptacja udziału w grze
		memcpy(&newGameRequestPayload, packet->payload, sizeof(AMCOM_NewGameRequestPayload));
		printf("Responding with NEW_GAME_RESPONSE\n");
		bytesToSend = AMCOM_Serialize(AMCOM_NEW_GAME_RESPONSE, NULL, 0, amcomBuf);
		
	    break;
	case AMCOM_PLAYER_UPDATE_REQUEST:
		printf("Got PLAYER_UPDATE.request.\n");
		// TODO: use the received information
		
		// load information about player update
		memcpy(&playerUpdateRequestPayload, packet->payload, packet->header.length);
		

	    break;
	case AMCOM_FOOD_UPDATE_REQUEST:
		printf("Got FOOD_UPDATE.request.\n");
		// TODO: use the received information

		// ładujemy informacje o food update
		if (isFirstCall == true) 
		{
			memcpy(&foodUpdateRequestPayload, packet->payload, packet->header.length);
			isFirstCall = false;
		} 
		else 
		{
			memcpy(&tempFood, packet->payload, packet->header.length);
			for (int i = 0; i < (packet->header.length / 11); ++i)
			{
				for (int j = 0; j < AMCOM_MAX_FOOD_UPDATES; ++j)
				{
					if (tempFood.foodState[i].foodNo == foodUpdateRequestPayload.foodState[j].foodNo)
					{
						memcpy(&foodUpdateRequestPayload.foodState[j], &tempFood.foodState[i], 11);
						//break;
					}
				}
			}
		}

		break;
	case AMCOM_MOVE_REQUEST:
		printf("Got MOVE.request.\n");
		// TODO: respond with MOVE.confirmation
		memcpy(&moveRequestPayload, packet->payload, packet->header.length);

		// unrealistic values
		lowDistance = 999999999999.0;
		posX = -100000000.0;
		posY = -100000000.0;

		// check distances to food
		optimalFood(&foodUpdateRequestPayload, &moveRequestPayload, &posX, &posY, &isAnyFoodLeft);

		closestRivalHp = 0;
		// if there is any player left, send the angle
		if (isAnyFoodLeft)
		{
			moveResponsePayload.angle = calculateAngle(posX, posY, moveRequestPayload.x, moveRequestPayload.y);	
		}
		// if there is no food left
		else
		{
			// calculating move response angle
			moveResponsePayload.angle = moveStrengthEvaluation(&playerUpdateRequestPayload, &moveRequestPayload, &posX, &posY, isAnyFoodLeft);
			
		}
		


		// send angle to game
		bytesToSend = AMCOM_Serialize(AMCOM_MOVE_RESPONSE, &moveResponsePayload, sizeof(AMCOM_MoveResponsePayload), amcomBuf);
		isAnyFoodLeft = false;
		break;

	}

	if (bytesToSend > 0) {
		int bytesSent = send(sock, (const char*)amcomBuf, bytesToSend, 0 );
		if (bytesSent == SOCKET_ERROR) {
			printf("Socket send failed with error: %d\n", WSAGetLastError());
			closesocket(sock);
			return;
		} else {
			printf("Sent %d bytes through socket.\n", bytesSent);
		}
	}
}

DWORD WINAPI playerThread( LPVOID lpParam )
{
	AMCOM_Receiver amcomReceiver;		// AMCOM receiver structure
	SOCKET sock = (SOCKET)(lpParam);	// socket used for communication with game client
	char buf[512];						// buffer for temporary data
	int receivedBytesCount;				// holds the number of bytes received via socket

	printf("Got new TCP connection.\n");

	// Initialize AMCOM receiver
	AMCOM_InitReceiver(&amcomReceiver, amcomPacketHandler, (void*)sock);

	// Receive data from socket until the peer shuts down the connection
	do {
		// Fetch the bytes from socket into buf
		receivedBytesCount = recv(sock, buf, sizeof(buf), 0);
		if (receivedBytesCount > 0) {
			printf("Received %d bytes in socket.\n", receivedBytesCount);
			// Try to deserialize the incoming data
			AMCOM_Deserialize(&amcomReceiver, buf, receivedBytesCount);
		} else if (receivedBytesCount < 0) {
			// Negative result indicates that there was socket communication error
			printf("Socket recv failed with error: %d\n", WSAGetLastError());
			closesocket(sock);
			break;
		}
	} while (receivedBytesCount > 0);

	printf("Closing connection.\n");

	// shutdown the connection since we're done
	receivedBytesCount = shutdown(sock, SD_SEND);
	// cleanup
	closesocket(sock);

	return 0;
}

int main(int argc, char **argv) {
	WSADATA wsaData;						// socket library data
	SOCKET listenSocket = INVALID_SOCKET;	// socket on which we will listen for incoming connections
	SOCKET clientSocket = INVALID_SOCKET;	// socket for actual communication with the game client
	struct addrinfo *addrResult = NULL;
	struct addrinfo hints;
	int result;

	// Say hello
	printf("mniAM player listening on port %s\nPress CTRL+x to quit\n", DEFAULT_TCP_PORT);

	// Initialize Winsock
	result = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (result != 0) {
		printf("WSAStartup failed with error: %d\n", result);
		return -1;
	}

	// Prepare hints structure
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	result = getaddrinfo(NULL, DEFAULT_TCP_PORT, &hints, &addrResult);
	if ( result != 0 ) {
		printf("Function 'getaddrinfo' failed with error: %d\n", result);
		WSACleanup();
		return -2;
	}

	// Create a socket for connecting to server
	listenSocket = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
	if (listenSocket == INVALID_SOCKET) {
		printf("Function 'socket' failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(addrResult);
		WSACleanup();
		return -3;
	}
	// Setup the TCP listening socket
	result = bind(listenSocket, addrResult->ai_addr, (int)addrResult->ai_addrlen);
	if (result == SOCKET_ERROR) {
		printf("Function 'bind' failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(addrResult);
		closesocket(listenSocket);
		WSACleanup();
		return -4;
	}
	freeaddrinfo(addrResult);

	// Listen for connections
	result = listen(listenSocket, SOMAXCONN);
	if (result == SOCKET_ERROR) {
		printf("Function 'listen' failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return -5;
	}

	while (1) {
		// Accept client socket
		clientSocket = accept(listenSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET) {
			printf("Function 'accept' failed with error: %d\n", WSAGetLastError());
			closesocket(listenSocket);
			WSACleanup();
			return -6;
		} else {
			// Run a separate thread to handle the actual game communication
			CreateThread(NULL, 0, playerThread, (void*)clientSocket, 0, NULL);
		}
		Sleep(10);
	}

	// No longer need server socket
	closesocket(listenSocket);
	// Deinitialize socket library
	WSACleanup();
	// We're done
	return 0;
}
