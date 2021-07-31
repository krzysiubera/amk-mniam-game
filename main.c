#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "usart.h"
#include "amcom.h"
#include "amcom_packets.h"
#include "math.h"
#include "stm32f4xx_hal.h"

#define PI 3.1415926
#define DEAD_OPPONENT_DIST 2137.0f

#define DEFAULT_TCP_PORT 	"2001"

bool wasGameClosed = false;
uint8_t ourPlayerID;

struct Point
{
	float x;
	float y;
};

float calculateDistance(float x_food, float y_food, float x_player, float y_player)
{
	float dist;
	dist = ((x_player-x_food)*(x_player-x_food)) + ((y_player-y_food)*(y_player-y_food));
	dist =  sqrt(dist);
	return dist;
}

float calculateAngle(float x_food, float y_food, float x_player, float y_player)
{
	float ang;
	ang = atan2(y_food - y_player, x_food - x_player);
	if (ang < 0)
		ang += 2 * PI;
	return ang;
}

// function which takes our and opponent position and returns angle to intercept
// Firstly we wanted to implement intercepting course, but finally we have decieded to use pursuing curve
float intercept (float playerX, float playerY, float opponentX, float opponentY, float opponentMoveAngle)
{
	return calculateAngle(opponentX, opponentY, playerX, playerY);
}

/*  !!!OBSOLETE!!!
*	Example usage:
*	float *intercept = interceptPoint(300, 300, 400, 400, calculateAngle(300, 300, 400, 400));
*   printf("X:   %f\nY:    %f", *intercept, *(intercept + 1));
*
*	@param returns pointer to array of X, Y of intercept point
*/

struct Point interceptPoint (float playerX, float playerY, float opponentX, float opponentY, float opponentMoveAngle)
{
	float middleX = (playerX + opponentX) / 2;
	float middleY = (playerY + opponentY) / 2;

	float angleToOpponent = calculateAngle(middleX, middleY, playerX, playerY);

	if (angleToOpponent == 0)
	{
		struct Point opponentLocation;
		opponentLocation.x = opponentX;
		opponentLocation.y = opponentY;
		return opponentLocation;
	}
	
	float a1 = tan(angleToOpponent);	// a = tan(alpha) -> slope of linear function
	
	float A1 = -1 / a1;					// perpendicular line equation 
    float A2 = opponentMoveAngle;

	float C1 = middleY - A1*middleX;			// the equation of the line passing through the middle point (y = a2*x + b)
	float C2 = opponentY - A2*opponentX;

	// http://www.math.edu.pl/punkt-przeciecia-dwoch-prostych
	// calculating point of interception
	float W = A1 - A2;					// determinant (since B factors are equal to 1)
	float Wx = C2 - C1;
	float Wy = C1*A2 - A1*C2;

	if (W == 0)
	{
		struct Point opponentLocation;
		opponentLocation.x = opponentX;
		opponentLocation.y = opponentY;
		return opponentLocation;
	}

	struct Point pointOfInterception;

	pointOfInterception.x = Wx/W;
	pointOfInterception.y = Wy/W;

	return pointOfInterception;
}

// This function returns position to the closest food. Calculates best path for 2 closest transistors
void optimalFood (AMCOM_FoodUpdateRequestPayload *foodUpdateRequestPayload, AMCOM_MoveRequestPayload *moveRequestPayload, float *posX, float *posY, bool *isAnyFoodLeft)
{
	// check distances to food
	float foodDistanceToFood, tempDistance, sumOfDistances, lowestSumOfDistances = 999999999999.0;
	int temp = -1;

	for (int i = 0; i < AMCOM_MAX_FOOD_UPDATES; ++i)
	{
		if (foodUpdateRequestPayload->foodState[i].state == 1)
		{
			tempDistance = calculateDistance(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, 
												moveRequestPayload->x, moveRequestPayload->y);

			temp = i;		// in case that there is only 1 food left
			// Depth 2
			// calculation of distances between food
			for (int j = 0; j < AMCOM_MAX_FOOD_UPDATES; ++j)
			{
				if (j == i)
					continue;

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

	// if there is only 1 food left, no more no less
	if (!(*isAnyFoodLeft) && temp != -1)
	{
		*posX = foodUpdateRequestPayload->foodState[temp].x;
		*posY =  foodUpdateRequestPayload->foodState[temp].y;
		*isAnyFoodLeft = true;
	}
}

// This function works like optimalFood function, but also checks if any other player is closer to this food than us, and if he is, change target food
void optimalFoodCheck (AMCOM_FoodUpdateRequestPayload *foodUpdateRequestPayload, AMCOM_MoveRequestPayload *moveRequestPayload, float *posX, float *posY, bool *isAnyFoodLeft, 
					float opponentX, float opponentY, float opponentAngle)
{
	// check distances to food
	float foodDistanceToFood, tempDistance, sumOfDistances, lowestSumOfDistances = 999999999999.0;
	int temp = -1;

	for (int i = 0; i < AMCOM_MAX_FOOD_UPDATES; ++i)
	{
		if (foodUpdateRequestPayload->foodState[i].state == 1)
		{
			tempDistance = calculateDistance(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, 
												moveRequestPayload->x, moveRequestPayload->y);

			temp = i;		// in case that there is only 1 food left
			
			// Depth 2
			// calculation of distances between food
			for (int j = 0; j < AMCOM_MAX_FOOD_UPDATES; ++j)
			{
				if (j == i)
					continue;

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
							// if closest player is closer to food than us and he is moving towards is, avoid (much better would)
							float opponentAngleToOurFood = calculateAngle(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, opponentX, opponentY);
							
							// if out opponent is dead, opponentX is set to DEAD_OPPONENT_DIST
							if (opponentX != DEAD_OPPONENT_DIST)
							{
								if (tempDistance > calculateDistance(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, opponentX, opponentY)
									&& ((opponentAngle < opponentAngleToOurFood + 0.5) || (opponentAngle > opponentAngleToOurFood - 0.5)))
								{
									continue;
								}
							}
							*posX = foodUpdateRequestPayload->foodState[i].x;
							*posY = foodUpdateRequestPayload->foodState[i].y;
							*isAnyFoodLeft = true;
						}
						else
						{
							// if closest player is closer to food than us and he is moving towards is, avoid (much better would)
							float opponentAngleToOurFood = calculateAngle(foodUpdateRequestPayload->foodState[j].x, foodUpdateRequestPayload->foodState[j].y, opponentX, opponentY);
							
                            // if out opponent is dead, opponentX is set to DEAD_OPPONENT_DIST
							if (opponentX != DEAD_OPPONENT_DIST)
							{
                                if (tempDistance > calculateDistance(foodUpdateRequestPayload->foodState[j].x, foodUpdateRequestPayload->foodState[j].y, opponentX, opponentY)
                                    && ((opponentAngle < opponentAngleToOurFood + 0.5) || (opponentAngle > opponentAngleToOurFood - 0.5)))
                                {
                                    continue;
                                }
                            }
							*posX = foodUpdateRequestPayload->foodState[j].x;
							*posY = foodUpdateRequestPayload->foodState[j].y;
							*isAnyFoodLeft = true;
						}
					}
				}
			}
		}
	}

	// if there is only 1 food left, no more no less
	if (!(*isAnyFoodLeft) && temp != -1)
	{
		*posX = foodUpdateRequestPayload->foodState[temp].x;
		*posY =  foodUpdateRequestPayload->foodState[temp].y;
		*isAnyFoodLeft = true;
	}
}

/*
* 	This function has to consider, whether chasing player is a danger for us, should we attack, or 
*	rather be interested in eating food.
*	
*	@param returns angle in radians
*/

float moveStrengthEvaluation (AMCOM_PlayerUpdateRequestPayload *playerUpdateRequestPayload, AMCOM_MoveRequestPayload *moveRequestPayload, 
							AMCOM_FoodUpdateRequestPayload *foodUpdateRequestPayload, float *posX, float *posY, bool *isAnyFoodLeft)
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

	// if game was restarted, flush. I have no idea, how to pass arguments betwen threads in C, so it will repeat as long as player won't consume at least 1 transistor 
	if (wasGameClosed)
	{
		memset(playerPreviousPositionX, 0, AMCOM_MAX_PLAYER_UPDATES);
		memset(playerPreviousPositionY, 0, AMCOM_MAX_PLAYER_UPDATES);
        wasGameClosed = false;
	}
    
	for (int i = 0; i < AMCOM_MAX_PLAYER_UPDATES; i++)
	{
		if (playerUpdateRequestPayload->playerState[i].hp == 0 || playerUpdateRequestPayload->playerState[i].playerNo == ourPlayerID)
			continue;

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

	// prev positon initialized with 0.0000 by default
	float toOurPlayerAngle		= calculateAngle(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY);
	float closestRivalMoveAngle = calculateAngle(rivalPosX, rivalPosY, playerPreviousPositionX[closestRivalID], playerPreviousPositionY[closestRivalID]);

	for (int i = 0; i < AMCOM_MAX_PLAYER_UPDATES; ++i)
	{
		playerPreviousPositionX[i] = rivalPosX;
		playerPreviousPositionY[i] = rivalPosY;
	}


	if (closestRivalHp == 0)
		rivalPosX = DEAD_OPPONENT_DIST;

	// run away from him
	if (closestRivalHp > playerUpdateRequestPayload->playerState->hp)
	{
		// if toOurPlayerAngle is between +- 10 degree the direction of enemy player movement
		if (toOurPlayerAngle < closestRivalMoveAngle + 0.18 && toOurPlayerAngle > closestRivalMoveAngle - 0.18)
		{
			if (*isAnyFoodLeft)
			{
				// calculate optimal course if able, we want here to catch
				optimalFoodCheck(foodUpdateRequestPayload, moveRequestPayload, posX, posY, isAnyFoodLeft, rivalPosX, rivalPosY, closestRivalMoveAngle);
				return calculateAngle(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);	
			}
			else
			{
				optimalFoodCheck(foodUpdateRequestPayload, moveRequestPayload, posX, posY, isAnyFoodLeft, rivalPosX, rivalPosY, closestRivalMoveAngle);
				return calculateAngle(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY);
			}
		}
		else
		{
			optimalFoodCheck(foodUpdateRequestPayload, moveRequestPayload, posX, posY, isAnyFoodLeft, rivalPosX, rivalPosY, closestRivalMoveAngle);
			return calculateAngle(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);
		}
	}

	// if we have at least 1 hp more
	else if (closestRivalHp < playerUpdateRequestPayload->playerState->hp && closestRivalHp != 0)
	{
		// evalutaion of 
		if (*isAnyFoodLeft)
		{
			float distanceToClosestFood 	= calculateDistance(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);
			float distanceToClosestPlayer 	= calculateDistance(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY);

			// if closest player is closer than food
			if (distanceToClosestPlayer < distanceToClosestFood)
			{
				return intercept(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY, closestRivalMoveAngle);
			}
			// if food is closer
			else
			{
				optimalFoodCheck(foodUpdateRequestPayload, moveRequestPayload, posX, posY, isAnyFoodLeft, rivalPosX, rivalPosY, closestRivalMoveAngle);
				return calculateAngle(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);
			}
		}
		else
		{
			return intercept(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY, closestRivalMoveAngle);
		}

	}

	else
	{
		if (*isAnyFoodLeft)
		{

			optimalFoodCheck(foodUpdateRequestPayload, moveRequestPayload, posX, posY, isAnyFoodLeft, rivalPosX, rivalPosY, closestRivalMoveAngle);
			return calculateAngle(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);
		}
		else
		{
			return intercept(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY, closestRivalMoveAngle);
		}
	}
}

/**
 * This function will be called each time a valid AMCOM packet is received
 */
void amcomPacketHandler(const AMCOM_Packet* packet, void* userContext) {
	uint8_t amcomBuf[AMCOM_MAX_PACKET_SIZE];	// buffer used to serialize outgoing packets
	size_t bytesToSend = 0;						// size of the outgoing packet
	static int playerCounter;					// just a counter to distinguish player instances

	AMCOM_NewGameRequestPayload newGameRequestPayload;		
	AMCOM_MoveResponsePayload moveResponsePayload;			
	static AMCOM_PlayerUpdateRequestPayload playerUpdateRequestPayload;	
	static AMCOM_FoodUpdateRequestPayload foodUpdateRequestPayload;	
	AMCOM_FoodUpdateRequestPayload tempFood;
	AMCOM_MoveRequestPayload moveRequestPayload;

	static bool isFirstCall = true;
	float tempDistance;
	float lowDistance;
	float distancesArray[AMCOM_MAX_FOOD_UPDATES];
	float posX;
	float posY;
	bool isAnyFoodLeft = false; 
	float rivalPosX;
	float rivalPosY;
	float tempRivalDistance;
	float closestRivalDistance;

	switch (packet->header.type) {
	case AMCOM_IDENTIFY_REQUEST:
		;       // because compiler is stupid and can't compile the program without this empty line
		AMCOM_IdentifyResponsePayload identifyResponse;
		sprintf(identifyResponse.playerName, "Vozdushno-desantnye voj");
		bytesToSend = AMCOM_Serialize(AMCOM_IDENTIFY_RESPONSE, &identifyResponse, sizeof(identifyResponse), amcomBuf);
        isFirstCall = true;
		break;
	case AMCOM_NEW_GAME_REQUEST:
		// accept invitation to game
		memcpy(&newGameRequestPayload, packet->payload, sizeof(AMCOM_NewGameRequestPayload));
        ourPlayerID = newGameRequestPayload.playerNumber;
		bytesToSend = AMCOM_Serialize(AMCOM_NEW_GAME_RESPONSE, NULL, 0, amcomBuf);
	    break;
	case AMCOM_PLAYER_UPDATE_REQUEST:
		// load information about player update
		memcpy(&playerUpdateRequestPayload, packet->payload, packet->header.length);
	    break;
	case AMCOM_FOOD_UPDATE_REQUEST:
		// if it's first call, load all information about food
		if (isFirstCall == true) 
		{
			memcpy(&foodUpdateRequestPayload, packet->payload, packet->header.length);
            wasGameClosed = true;
			isFirstCall = false;
		} 
		// if it's not, load information about food but we don't want to have two the same instances of food
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
					}
				}
			}
		}

		break;
	case AMCOM_MOVE_REQUEST:
		memcpy(&moveRequestPayload, packet->payload, packet->header.length);

		// unrealistic values
		lowDistance = 999999999999.0;
		posX = -100000000.0;
		posY = -100000000.0;

		// check distances to food
		optimalFood(&foodUpdateRequestPayload, &moveRequestPayload, &posX, &posY, &isAnyFoodLeft);

		moveResponsePayload.angle = moveStrengthEvaluation(&playerUpdateRequestPayload, &moveRequestPayload, &foodUpdateRequestPayload, &posX, &posY, &isAnyFoodLeft);

		// send angle to game
		bytesToSend = AMCOM_Serialize(AMCOM_MOVE_RESPONSE, &moveResponsePayload, sizeof(AMCOM_MoveResponsePayload), amcomBuf);

		isAnyFoodLeft = false;
		break;

	}

	if (bytesToSend > 0) {
		USART_WriteData((const char*)amcomBuf, bytesToSend);
	}
}


int main(int argc, char **argv) {
    // init peripherials
	//HAL_Init();
    USART_Init();

    // init amcom
    AMCOM_Receiver amcomRec;

    char buf[512];
    size_t cnt = 0;


    AMCOM_InitReceiver(&amcomRec, amcomPacketHandler, NULL);

    // read data in loop and then deserialize it
    while(1)
    {
        cnt = USART_ReadData(buf, sizeof(buf));
        if (cnt > 0)
        {
            AMCOM_Deserialize(&amcomRec, buf, cnt);
        }
    }

}


void SysTick_Handler(void) {
    ;
}
