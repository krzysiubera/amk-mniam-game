#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "amcom.h"

/// Start of packet character
const uint8_t  AMCOM_SOP         = 0xA1;
const uint16_t AMCOM_INITIAL_CRC = 0xFFFF;

static uint16_t AMCOM_UpdateCRC(uint8_t byte, uint16_t crc)
{
	byte ^= (uint8_t)(crc & 0x00ff);
	byte ^= (uint8_t)(byte << 4);
	return ((((uint16_t)byte << 8) | (uint8_t)(crc >> 8)) ^ (uint8_t)(byte >> 4) ^ ((uint16_t)byte << 3));
}


void AMCOM_InitReceiver(AMCOM_Receiver* receiver, AMCOM_PacketHandler packetHandlerCallback, void* userContext) {
	if (receiver == NULL)
    {
        return;
    }
    
    receiver->payloadCounter = 0;
    receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
	receiver->packetHandler = packetHandlerCallback;
	receiver->userContext = userContext;
}

size_t AMCOM_Serialize(uint8_t packetType, const void* payload, size_t payloadSize, uint8_t* destinationBuffer) {
	// create handler to our packet header
	AMCOM_PacketHeader packetHeaderHandler;
	
	// initialize it
	packetHeaderHandler.sop = AMCOM_SOP;
	packetHeaderHandler.type = packetType;
	packetHeaderHandler.length = payloadSize;
	packetHeaderHandler.crc = AMCOM_INITIAL_CRC;
	
	// create handler to our packet
	AMCOM_Packet packetHandler;
	
	// initialize its header 
	packetHandler.header = packetHeaderHandler;
	
	// calculate CRC
	packetHeaderHandler.crc = AMCOM_UpdateCRC(packetHeaderHandler.type, packetHeaderHandler.crc);
	packetHeaderHandler.crc = AMCOM_UpdateCRC(packetHeaderHandler.length, packetHeaderHandler.crc);
	
    for (size_t counter = 0; counter < payloadSize; ++counter)
    {
        packetHeaderHandler.crc = AMCOM_UpdateCRC(((uint8_t*)payload)[counter], packetHeaderHandler.crc);
    }
	
	// write header to our destination buffer
	memcpy(destinationBuffer, &packetHeaderHandler, sizeof(AMCOM_PacketHeader));
	
	// write actual data to our destination buffer but move to appropriate place in memory
	memcpy(destinationBuffer + sizeof(AMCOM_PacketHeader), payload, payloadSize);
        
    // return information about bytes we write to destination buffer
    return payloadSize + sizeof(AMCOM_PacketHeader);
}

void AMCOM_Deserialize(AMCOM_Receiver* receiver, const void* data, size_t dataSize) {
    static uint8_t lowerNibble = 0;
    static uint8_t higherNibble = 0;
    uint16_t calculatedCRC = AMCOM_INITIAL_CRC;
            
    for (size_t i = 0; i < dataSize; ++i)
    {
        // we got some byte from the buffer
        uint8_t receivedByte = ((uint8_t*)data)[i];
        
        
        // and we have to make a decision about it
        switch (receiver->receivedPacketState)
        {
        case AMCOM_PACKET_STATE_EMPTY:
            // try to get our SOP
            if (receivedByte == AMCOM_SOP)
            {
                receiver->receivedPacket.header.sop = AMCOM_SOP;
                //receiver->payloadCounter = 0;
                receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_SOP;
            }
            break;
        case AMCOM_PACKET_STATE_GOT_SOP:
            // try to get type
            receiver->receivedPacket.header.type = receivedByte;
            receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_TYPE;
            break;
        case AMCOM_PACKET_STATE_GOT_TYPE:
            // try to get length
            if (receivedByte >= 0 && receivedByte <= AMCOM_MAX_PACKET_SIZE)
            {
                receiver->receivedPacket.header.length = receivedByte;
                receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_LENGTH;
            }
            else
            {
                receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
            }
            break;
        case AMCOM_PACKET_STATE_GOT_LENGTH:
            // try to get lower byte of crc
            lowerNibble = receivedByte;
            receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_CRC_LO;
            break;
        case AMCOM_PACKET_STATE_GOT_CRC_LO:
            // try to get higher byte of crc
            higherNibble = receivedByte;
            
            // write crc to header
            receiver->receivedPacket.header.crc = ( (higherNibble << 8) | lowerNibble );
            
            // check if we have no data in payload
            if (receiver->receivedPacket.header.length == 0)
            {
                receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;  
                // this goto is needed because otherwise I would be thrown out of the loop with iterator i 
                goto got_payload;
            }
            else
            {
                receiver->receivedPacketState = AMCOM_PACKET_STATE_GETTING_PAYLOAD;
            }
            break;
        case AMCOM_PACKET_STATE_GETTING_PAYLOAD:
            // if we got to there, we need to get data from payload and write it to payload
            
            receiver->receivedPacket.payload[receiver->payloadCounter] = receivedByte;
            receiver->payloadCounter++;
            
            // check if we have no more data to retreive
            if (receiver->payloadCounter == receiver->receivedPacket.header.length)
            {
                receiver->receivedPacketState =  AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
                goto got_payload;
            }
            break;
            
            
    got_payload:
        case AMCOM_PACKET_STATE_GOT_WHOLE_PACKET:
            // we got our packet so we're going to calculate CRC
            
            calculatedCRC = AMCOM_UpdateCRC(receiver->receivedPacket.header.type, calculatedCRC);
            calculatedCRC = AMCOM_UpdateCRC(receiver->receivedPacket.header.length, calculatedCRC);
            
            for (size_t j = 0; j < receiver->payloadCounter; ++j)
            {
                calculatedCRC = AMCOM_UpdateCRC(receiver->receivedPacket.payload[j], calculatedCRC);
            }
            
            
            
            
            // check if our calculations are ok
            if ( calculatedCRC == receiver->receivedPacket.header.crc )
            {
                // if yes, call the user's function
                receiver->packetHandler(&(receiver->receivedPacket), receiver->userContext);
            }
            
            
            //receiver->packetHandler(&(receiver->receivedPacket), receiver->userContext);
            
            // go back to empty
            receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
            
            receiver->payloadCounter = 0;
            //memset(&receiver->receivedPacket, 0, sizeof(AMCOM_Packet));
            
            break;
            
        }
    
    }
}
