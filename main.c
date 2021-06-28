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

#define DEFAULT_TCP_PORT 	"2001"

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
		ang += 6.28f;
	return ang;
}

float intercept (float playerX, float playerY, float opponentX, float opponentY, float opponentMoveAngle)
{
	float middleX = (playerX + opponentX) / 2;
	float middleY = (playerY + opponentY) / 2;

	//float angleToOpponent = calculateAngle(middleX, middleY, playerX, playerY);
	float angleToOpponent = calculateAngle(opponentX, opponentY, playerX, playerY);

	// Test
	return angleToOpponent;
	// end test

	if (angleToOpponent == 0)
		return angleToOpponent;
	
	float a1 = tan(angleToOpponent);	// a = tan(alpha) -> slope of linear function
	float a2 = -1 / a1;					// perpendicular line equation 

	float b2 = middleY - a2*middleX;			// the equation of the line passing through the middle point (y = a2*x + b)
	float b3 = opponentY - opponentMoveAngle*opponentX;

	// http://www.math.edu.pl/punkt-przeciecia-dwoch-prostych
	// calculating point of interception
	float W = a2 - opponentMoveAngle;				// determinant (since B factors are equal to 1)
	float Wx = b3 - b2;
	float Wy = opponentMoveAngle*b2 - b3*a2;

	if (W == 0)
		return angleToOpponent;

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

				/**/
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
							//printf("1)\n");
							// if closest player is closer to food than us and he is moving towards is, avoid (much better would)
							float opponentAngleToOurFood = calculateAngle(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, opponentX, opponentY);
							//printf("Odleglosc: %f\nOdleglosc przewidywana: %f\n---------------\n", tempDistance, calculateDistance(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, opponentX, opponentY));
							
							// if out opponent is dead, opponentX is set to -2137.0
							if (opponentX != -2137.0)
							{
								if (tempDistance > calculateDistance(foodUpdateRequestPayload->foodState[i].x, foodUpdateRequestPayload->foodState[i].y, opponentX, opponentY)
									&& ((opponentAngle < opponentAngleToOurFood + 0.5) || (opponentAngle > opponentAngleToOurFood - 0.5)))
								{
									//printf("skipped\n");
									//printf("i: %d\n", i);
									continue;
								}
							}
							//printf("i: %d, j: %d\n", i, j);
							*posX = foodUpdateRequestPayload->foodState[i].x;
							*posY = foodUpdateRequestPayload->foodState[i].y;
							*isAnyFoodLeft = true;
						}
						else
						{
							// if closest player is closer to food than us and he is moving towards is, avoid (much better would)
							float opponentAngleToOurFood = calculateAngle(foodUpdateRequestPayload->foodState[j].x, foodUpdateRequestPayload->foodState[j].y, opponentX, opponentY);
							if (tempDistance > calculateDistance(foodUpdateRequestPayload->foodState[j].x, foodUpdateRequestPayload->foodState[j].y, opponentX, opponentY)
								&& ((opponentAngle < opponentAngleToOurFood + 0.5) || (opponentAngle > opponentAngleToOurFood - 0.5)))
								{
									continue;
								}
							*posX = foodUpdateRequestPayload->foodState[j].x;
							*posY = foodUpdateRequestPayload->foodState[j].y;
							*isAnyFoodLeft = true;
						}
					}
				}

				/**/
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
	if (playerUpdateRequestPayload->playerState[0].hp == 2)
	{
		memset(playerPreviousPositionX, 0, AMCOM_MAX_PLAYER_UPDATES);
		memset(playerPreviousPositionY, 0, AMCOM_MAX_PLAYER_UPDATES);
	}

	for (int i = 1; i < AMCOM_MAX_PLAYER_UPDATES; i++)
	{
		if (playerUpdateRequestPayload->playerState[i].hp == 0)
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
	//float closestRivalMoveAngle = calculateAngle(playerPreviousPositionX[closestRivalID], playerPreviousPositionY[closestRivalID], rivalPosX, rivalPosY);
	//printf("Opp: %f\nUs: %f\n", closestRivalMoveAngle, toOurPlayerAngle);
	float toOurPlayerAngle		= calculateAngle(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY);
	float closestRivalMoveAngle = calculateAngle(rivalPosX, rivalPosY, playerPreviousPositionX[closestRivalID], playerPreviousPositionY[closestRivalID]);
	//float toOurPlayerAngle		= calculateAngle(rivalPosX, rivalPosY, moveRequestPayload->x, moveRequestPayload->y);
	

	for (int i = 0; i < AMCOM_MAX_PLAYER_UPDATES; ++i)
	{
		playerPreviousPositionX[i] = rivalPosX;
		playerPreviousPositionY[i] = rivalPosY;
	}


	if (closestRivalHp == 0)
		rivalPosX = 2137.0;

	// run away from him
	if (closestRivalHp > playerUpdateRequestPayload->playerState->hp)
	{
		// if toOurPlayerAngle is between +- 10 degree the direction of enemy player movement
		if (toOurPlayerAngle < closestRivalMoveAngle + 0.18 && toOurPlayerAngle > closestRivalMoveAngle - 0.18)
		{
			if (*isAnyFoodLeft)
			{
				// calculate optimal course if able, we want here to catch and 
				// ToDo
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
	
	// czy to aby na pewno tak się zachowuje? Czy gracze o tym samym HP ze sobą nie mają interakcji?
	// ToDo change
	// if we have at least 2 hp more
	else if (closestRivalHp + 1 < playerUpdateRequestPayload->playerState->hp && closestRivalHp != 0)
	{
		// evalutaion of 
		if (*isAnyFoodLeft)
		{
			float distanceToClosestFood 	= calculateDistance(*posX, *posY, moveRequestPayload->x, moveRequestPayload->y);
			float distanceToClosestPlayer 	= calculateDistance(moveRequestPayload->x, moveRequestPayload->y, rivalPosX, rivalPosY);


			// if closest player is closer than food
			if (distanceToClosestPlayer + 100 < distanceToClosestFood || closestRivalHp + 2 < playerUpdateRequestPayload->playerState->hp)
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
	// SOCKET sock = (SOCKET)userContext;			// socket used for communication with the game client

	// written by me
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
	//static uint16_t closestRivalHp;

	switch (packet->header.type) {
	case AMCOM_IDENTIFY_REQUEST:
		;
		AMCOM_IdentifyResponsePayload identifyResponse;
		sprintf(identifyResponse.playerName, "spetsnaz", playerCounter++);
		bytesToSend = AMCOM_Serialize(AMCOM_IDENTIFY_RESPONSE, &identifyResponse, sizeof(identifyResponse), amcomBuf);
		break;
	case AMCOM_NEW_GAME_REQUEST:
		//printf("Got NEW_GAME.request.\n");
		// TODO: respond with NEW_GAME.confirmation

		// akceptacja udziału w grze
		memcpy(&newGameRequestPayload, packet->payload, sizeof(AMCOM_NewGameRequestPayload));
		bytesToSend = AMCOM_Serialize(AMCOM_NEW_GAME_RESPONSE, NULL, 0, amcomBuf);
		
	    break;
	case AMCOM_PLAYER_UPDATE_REQUEST:
		//printf("Got PLAYER_UPDATE.request.\n");
		// TODO: use the received information
		
		// load information about player update
		memcpy(&playerUpdateRequestPayload, packet->payload, packet->header.length);
		

	    break;
	case AMCOM_FOOD_UPDATE_REQUEST:
		//printf("Got FOOD_UPDATE.request.\n");
		// TODO: use the received information
		// printf("Food\n");
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
						// kopiuje informacje o jedzeniu do tablicy foodState
						memcpy(&foodUpdateRequestPayload.foodState[j], &tempFood.foodState[i], 11);
						//break;
					}
				}
			}
		}

		break;
	case AMCOM_MOVE_REQUEST:
		//printf("Got MOVE.request.\n");
		// TODO: respond with MOVE.confirmation
		memcpy(&moveRequestPayload, packet->payload, packet->header.length);

		// unrealistic values
		lowDistance = 999999999999.0;
		posX = -100000000.0;
		posY = -100000000.0;

		// check distances to food
		optimalFood(&foodUpdateRequestPayload, &moveRequestPayload, &posX, &posY, &isAnyFoodLeft);

		// if there is any player left, send the angle
		// tu trzeba będzie ewaluować, czy opłaca się kogoś gonić, czy 
		/*if (isAnyFoodLeft)
		{
			// + unik
			moveResponsePayload.angle = calculateAngle(posX, posY, moveRequestPayload.x, moveRequestPayload.y);	
		}
		// if there is no food left
		else
			moveResponsePayload.angle = moveStrengthEvaluation(&playerUpdateRequestPayload, &moveRequestPayload, &posX, &posY, isAnyFoodLeft);*/

		moveResponsePayload.angle = moveStrengthEvaluation(&playerUpdateRequestPayload, &moveRequestPayload, &foodUpdateRequestPayload, &posX, &posY, &isAnyFoodLeft);

		// send angle to game
		bytesToSend = AMCOM_Serialize(AMCOM_MOVE_RESPONSE, &moveResponsePayload, sizeof(AMCOM_MoveResponsePayload), amcomBuf);
		//printf("Bytes: %d\n", bytesToSend);
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
