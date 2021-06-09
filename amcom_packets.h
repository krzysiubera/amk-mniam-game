#ifndef AMCOM_PACKETS_H_
#define AMCOM_PACKETS_H_

#include "amcom.h"

/// Maximum length of the player name including trailing '\0'
#define AMCOM_MAX_PLAYER_NAME_LEN	24
/// Maximum number of @ref AMCOM_PlayerState structures in a PLAYER_UPDATE.request packet
#define AMCOM_MAX_PLAYER_UPDATES	8
/// Maximum number of @ref AMCOM_FoodState structures in a FOOD_UPDATE.request packet
#define AMCOM_MAX_FOOD_UPDATES		16

/// Possible packet types
typedef enum {
	AMCOM_NO_PACKET = 0,
	AMCOM_IDENTIFY_REQUEST = 1,
	AMCOM_IDENTIFY_RESPONSE = 2,
	AMCOM_NEW_GAME_REQUEST = 3,
	AMCOM_NEW_GAME_RESPONSE = 4,
	AMCOM_PLAYER_UPDATE_REQUEST = 5,
	AMCOM_FOOD_UPDATE_REQUEST = 7,
	AMCOM_MOVE_REQUEST = 9,
	AMCOM_MOVE_RESPONSE = 10,
} AMCOM_PacketType;

/// Structure of the IDENTIFY.request packet payload
typedef struct AMPACKED {
	/// Game version - higher version number
	uint8_t gameVerHi;
	/// Game version - lower version number
	uint8_t gameVerLo;
	/// Game revision
	uint16_t gameRevision;
} AMCOM_IdentifyRequestPayload;
// static assertion to check that the structure is indeed packed
static_assert(4 == sizeof(AMCOM_IdentifyRequestPayload), "4 != sizeof(AMCOM_IdentifyRequestPayload)");


/// Structure of the IDENTIFY.response packet payload
typedef struct AMPACKED {
	/// Player name (including trailing '\0')
	char playerName[AMCOM_MAX_PLAYER_NAME_LEN];
} AMCOM_IdentifyResponsePayload;
// static assertion to check that the structure is indeed packed
static_assert(24 == sizeof(AMCOM_IdentifyResponsePayload), "24 != sizeof(AMCOM_IdentifyResponsePayload)");

/// Structure of the NEW_GAME.request packet payload
typedef struct AMPACKED {
	uint8_t playerNumber;
	uint8_t numberOfPlayers;
	float mapWidth;
	float mapHeight;
} AMCOM_NewGameRequestPayload;
// static assertion to check that the structure is indeed packed
static_assert(10 == sizeof(AMCOM_NewGameRequestPayload), "10 != sizeof(AMCOM_NewGameRequestPayload)");

/// Structure describing the state of a single player
typedef struct AMPACKED {
	/// Player number
	uint8_t playerNo;
	/// Health points (0 means 'dead')
	uint16_t hp;
	/// X position on map
	float x;
	/// Y position on map
	float y;
} AMCOM_PlayerState;
// static assertion to check that the structure is indeed packed
static_assert(11 == sizeof(AMCOM_PlayerState), "11 != sizeof(AMCOM_PlayerState)");

/// Structure of the PLAYER_UPDATE.request packet payload
typedef struct AMPACKED {
	/// array of player states - the actual number of items in this array depends on the packet length
	AMCOM_PlayerState playerState[AMCOM_MAX_PLAYER_UPDATES];
} AMCOM_PlayerUpdateRequestPayload;
// static assertion to check that the structure is indeed packed
static_assert(88 == sizeof(AMCOM_PlayerUpdateRequestPayload), "88 != sizeof(AMCOM_PlayerUpdateRequestPayload)");

/// Structure describing the state of a single food
typedef struct AMPACKED {
	// Food number
	uint16_t foodNo;
	// Food state (1 = available, 0 = eaten)
	uint8_t state;
	/// X position on map
	float x;
	/// Y position on map
	float y;
} AMCOM_FoodState;
// static assertion to check that the structure is indeed packed
static_assert(11 == sizeof(AMCOM_FoodState), "11 != sizeof(AMCOM_FoodState)");

/// Structure of the FOOD_UPDATE.request packet payload
typedef struct AMPACKED {
	/// array of food states - the actual number of items in this array depends on the packet length
	AMCOM_FoodState foodState[AMCOM_MAX_FOOD_UPDATES];
} AMCOM_FoodUpdateRequestPayload;
// static assertion to check that the structure is indeed packed
static_assert(176 == sizeof(AMCOM_FoodUpdateRequestPayload), "176 != sizeof(AMCOM_FoodUpdateRequestPayload)");

/// Structure of the MOVE.request packet payload
typedef struct AMPACKED {
	/// current player position on map - X
	float x;
	/// current player position on map - Y
	float y;
} AMCOM_MoveRequestPayload;
// static assertion to check that the structure is indeed packed
static_assert(8 == sizeof(AMCOM_MoveRequestPayload), "8 != sizeof(AMCOM_MoveRequestPayload)");

/// Structure of the MOVE.response packet payload
typedef struct AMPACKED {
	/// angle at which the player should move (in radians)
	float angle;
} AMCOM_MoveResponsePayload;
// static assertion to check that the structure is indeed packed
static_assert(4 == sizeof(AMCOM_MoveResponsePayload), "4 != sizeof(AMCOM_MoveResponsePayload)");

#endif /* AMCOM_PACKETS_H_ */