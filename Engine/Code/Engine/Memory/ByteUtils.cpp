#include "Engine/Memory/ByteUtils.hpp"


EndianMode GetLocalMachineEndianness()
{
	union //Compiles out because it's all constant.
	{
		byte_t bdata[ 4 ];
		uint32_t uidata;
	} data;

	data.uidata = 0x04030201;

	//If first byte is the least significant, we know it's little-endian: the "little end" was first.
	return ( data.bdata[ 0 ] == 0x01 ) ? LITTLE_ENDIAN : BIG_ENDIAN;
	//End Method 2. Method 1 is commented out in the .hpp.
}


//--------------------------------------------------------------------------------------------------------------
void memcpy_backwards( void* dest, void const* src, size_t numBytesToCopy )
{
	//Not memcpy_r, which goes from the end and to the start of the buffer, but preserves ordering. -- that's useful for moving things, not a byte swap.
	byte_t* destAsByte = (byte_t*)dest;
	byte_t* currentByteToRead = (byte_t*)src + numBytesToCopy; //Starting at back to move backwards along.

	for ( size_t index = 0; index < numBytesToCopy; ++index )
	{
		--currentByteToRead;
		*destAsByte = *currentByteToRead;
		++destAsByte;
	}
	//KEY TAKEAWAY: can't use -- and ++ on a void* silly!
}