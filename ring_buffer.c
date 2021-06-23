/* Includes ------------------------------------------------------------------*/
#include <assert.h>
#include "ring_buffer.h"


bool RingBuffer_Init(RingBuffer *ringBuffer, char *dataBuffer, size_t dataBufferSize) 
{
	assert(ringBuffer);
	assert(dataBuffer);
	assert(dataBufferSize > 0);
	
	if ((ringBuffer) && (dataBuffer) && (dataBufferSize > 0)) {
	    // set all parameters to default values or values we got as parameters of function
	    ringBuffer->data = dataBuffer;
	    ringBuffer->head = 0;
	    ringBuffer->tail = 0;
	    ringBuffer->capacity = dataBufferSize;
	    ringBuffer->countElements = 0;
	    return true;
	}
	
	return false;
}

bool RingBuffer_Clear(RingBuffer *ringBuffer)
{
	assert(ringBuffer);
	
	if (ringBuffer) {
	    // set appropriate values to default
	    ringBuffer->head = 0;
	    ringBuffer->tail = 0;
	    ringBuffer->countElements = 0;
	    
	    return true;
	}
	return false;
}

bool RingBuffer_IsEmpty(const RingBuffer *ringBuffer)
{
  assert(ringBuffer);	
	if (ringBuffer->countElements == 0)
	    return true;
	else
	    return false;
}

size_t RingBuffer_GetLen(const RingBuffer *ringBuffer)
{
	assert(ringBuffer);
	
	if (ringBuffer) {
		
		return (ringBuffer->countElements);
		
	}
	return 0;
	
}

size_t RingBuffer_GetCapacity(const RingBuffer *ringBuffer)
{
	assert(ringBuffer);
	
	if (ringBuffer) {
		return ringBuffer->capacity;
	}
	return 0;	
}


bool RingBuffer_PutChar(RingBuffer *ringBuffer, char c)
{
	assert(ringBuffer);
	
	if (ringBuffer) {
		
		// we check if buffer is full
		if ((ringBuffer->countElements) == (ringBuffer->capacity))
		{
		    // cannot put char if buffer is full
		    return false;
		}
		    
		
		// add item to location
	    ringBuffer->data[ringBuffer->head] = c;
	    
	    // adjust head location by one
	    ringBuffer->head = (ringBuffer->head + 1) % (ringBuffer->capacity);
	    
	    // increase number of elements in buffer
	    (ringBuffer->countElements)++;
	    return true;
		
	}
	return false;
}

bool RingBuffer_GetChar(RingBuffer *ringBuffer, char *c)
{
	assert(ringBuffer);
	assert(c);
	
  if ((ringBuffer) && (c)) {
		
		// we check if buffer is empty
		if (RingBuffer_IsEmpty(ringBuffer))
		{
		    // cannot get char from empty buffer
		    return false;
		}
		
		// get item from data buffer
		*c = ringBuffer->data[ringBuffer->tail];
		
		
		// adjust tail position
		ringBuffer->tail = (ringBuffer->tail + 1) % (ringBuffer->capacity);
		
		// decrease number of elements in buffer
		(ringBuffer->countElements)--;
		
		return true;
		
	}
	
	return false;
}
