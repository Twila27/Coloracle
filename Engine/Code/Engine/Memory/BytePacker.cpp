#include "Engine/Memory/BytePacker.hpp"


//--------------------------------------------------------------------------------------------------------------
void BytePacker::WriteForwardAlongBuffer( void const* data, const size_t dataSize )
{
	memcpy( GetIoHead(), data, dataSize );
	AdvanceOffset( dataSize );
}


//--------------------------------------------------------------------------------------------------------------
void BytePacker::WriteBackwardAlongBuffer( void const* data, const size_t dataSize )
{
	memcpy_backwards( GetIoHead(), data, dataSize );
	AdvanceOffset( dataSize );
}


//--------------------------------------------------------------------------------------------------------------
const char* BytePacker::ReadString() const
{
	//Return ptr from inside m_buffer and keep its null-termination.
	if ( GetTotalReadableBytes() < 1 )
		return nullptr;

	byte_t firstChar;
	Read<byte_t>( &firstChar );

	if ( firstChar == INVALID_STRING_TOKEN )
	{
		return nullptr; //0xFF <=> nullptr, see WriteString.
	}
	else
	{
		char* buffer = (char*)GetIoHead() - 1; //-1 because we just read forward one above.
			//GetHead() returns the ptr to the buffer + its current offset.
		size_t maxSize = GetTotalReadableBytes() + 1;
		size_t len = 0;
		char* c = buffer;
		while ( ( len < maxSize ) && ( *c != NULL ) )
		{
			++len;
			++c;
		}
		
		//If c got to null, we have a nice string and can continue:
		if ( len < maxSize ) //i.e. c actually got to null.
		{
			AdvanceOffset( len );
			return buffer;
		}
		else
		{
			return nullptr; //May also want to advance( len ) to the end of it as well.
		}
	}

	//Example use: if buffer consists of [1,0,0,0,"hello",null,2,0,0,0] 
	//then ReadString() actually returns a pointer to the start of null-terminated "hello", 
	//but since the pointer return is valid we can just use it there (or copy it off). 
	//Usually you're only using the string while processing the message, if you need it longer, copy it off, 
	//e.g. the ping's string will just be echoed at that point in time it's read and then we're done.
}


//--------------------------------------------------------------------------------------------------------------
bool BytePacker::WriteString( const char* str )
{
	//More concerned about bandwidth here, writing out its length is too fat for us.

	//0xFF will never exist in ASCII (and utf-8 should be our format anywhere) so it's our nullptr-standin.
	if ( str == nullptr )
	{
		return Write<byte_t>( INVALID_STRING_TOKEN );
	}
	else
	{
		size_t len = strlen( str ) + 1; //+1 because though "" has strlen 0, it takes 1 byte in memory to store that.

		if ( ( m_ioOffset + len ) > m_maxWriteSize )
			return false; //Too big for m_buffer to hold.

		WriteForwardAlongBuffer( str, len ); //No need to worry about endian mode of a character array, because sizeof(char) == 1 byte!
			//Unless we're doing UTF-16, that is, but we're doing UTF-8 here.
		return true; //Can't really get a value out of the memcpy.
	}
	//Since we still want it null-terminated, so 1 byte min.
}


//--------------------------------------------------------------------------------------------------------------
void BytePacker::ReadForwardAlongBuffer( void* out_data, const size_t dataSize ) const
{
	memcpy( out_data, GetIoHead(), dataSize );
}


//--------------------------------------------------------------------------------------------------------------
void BytePacker::ReadBackwardAlongBuffer( void* out_data, const size_t dataSize ) const
{
	memcpy_backwards( out_data, GetIoHead(), dataSize );
}
