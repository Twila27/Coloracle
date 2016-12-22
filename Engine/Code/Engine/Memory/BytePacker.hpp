#pragma once


#include "Engine/Memory/ByteUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Vector2.hpp"


//-----------------------------------------------------------------------------
#define INVALID_STRING_TOKEN (0xFF)
typedef uint8_t ByteBufferBookmark; //Not a byte_t, we still want them to Reset() and then AdvanceLength( this bookmark ) when using Reserve().

//-----------------------------------------------------------------------------
class BytePacker //See BinaryWriter for a tool that copies data and byte swaps the copy in-place, rather than writing backwards.
{
protected:

public:
	BytePacker( size_t bufferSize, EndianMode packerEndianness = BIG_ENDIAN/*FOR A2*/ ) 
		: m_buffer( nullptr )
		, m_maxWriteSize( bufferSize ) 
		, m_maxReadSize( 0 )
		, m_packerEndianness( packerEndianness )
		, m_ioOffset( 0 )
	{
	}
	byte_t* GetBuffer() const { return m_buffer; }
	byte_t* GetIoHead() const { return m_buffer + m_ioOffset; }
	
	size_t GetWritableBytes() const { return GetTotalWritableBytes() - m_ioOffset; } //How much is left to write to.
	
	size_t GetTotalReadableBytes() const { return GetMax( m_ioOffset, m_maxReadSize ); } //Ensure maxReadSize updates on write!
	size_t GetTotalWritableBytes() const { return m_maxWriteSize; }

	template <typename DataType> ByteBufferBookmark ReserveForWriting( const DataType& data ); //Returns an offset to the current I/O head location for writing to later.
	template <typename DataType> bool WriteAtBookmark( ByteBufferBookmark offset, const DataType& data );
	template <typename DataType> bool Write( const DataType& data );
	void WriteForwardAlongBuffer( void const* data, const size_t dataSize );
	void WriteBackwardAlongBuffer( void const* data, const size_t dataSize );
	//Forward and backward are relative to this local machine's endianness.
	bool WriteString( const char* str ); //Separate to write more than just the base pointer.

	template <typename DataType> ByteBufferBookmark ReserveForReading( DataType* out_data ) const;
	template <typename DataType> bool ReadAtBookmark( ByteBufferBookmark offset, DataType* out_data ) const;
	template <typename DataType> bool Read( DataType* out_data ) const;
	void ReadForwardAlongBuffer( void* out_data, const size_t dataSize ) const;
	void ReadBackwardAlongBuffer( void* out_data, const size_t dataSize ) const;
	const char* ReadString() const; //Return ptr from inside m_buffer and keep its null-termination. Does advance offset. 
		//strcpy out if necessary to hold onto it for more than just Recv().

	void SetBuffer( void* buffer ) { m_buffer = (byte_t*)buffer; }
	void AdvanceOffset( size_t delta ) const { m_ioOffset += delta; }
	void SetTotalReadableBytes( size_t newReadHead ) { m_maxReadSize = newReadHead; }
	void ResetOffset( size_t newIoHeadPosition = 0 ) const { m_ioOffset = newIoHeadPosition; }

private:
	byte_t* m_buffer;
	size_t m_maxWriteSize; //How much is valid data exists in m_buffer for writing to.
	size_t m_maxReadSize; //How much is valid data exists in m_buffer that has been written to.
		//Affords error checking during the recv's read.
	EndianMode m_packerEndianness;
	mutable size_t m_ioOffset; //Where I'm currently writing or reading the buffer.
		//Combined in one variable: you're either only writing or only reading.
};


//--------------------------------------------------------------------------------------------------------------
template <typename DataType> ByteBufferBookmark BytePacker::ReserveForWriting( const DataType& data )
{
	ByteBufferBookmark marker = (ByteBufferBookmark)m_ioOffset;

	Write<DataType>( data );

	return marker; //So we can write over that data later.
}


//--------------------------------------------------------------------------------------------------------------
template <typename DataType> bool BytePacker::WriteAtBookmark( ByteBufferBookmark offset, const DataType& data )
{
	ByteBufferBookmark offsetBeforeRewind = (ByteBufferBookmark)m_ioOffset;

	ResetOffset( offset );
	bool success = Write<DataType>( data );

	ResetOffset( offsetBeforeRewind );

	return success;
}


//--------------------------------------------------------------------------------------------------------------
template <typename DataType> bool BytePacker::Write( const DataType& data ) //Use WriteString for const char*, not this!
{
	//No need to make a copy like in BinaryWriter!
	size_t dataSize = sizeof( DataType );

	if ( ( m_ioOffset + dataSize ) > m_maxWriteSize )
		return false; //Too big for m_buffer to hold.

	//Forward and backward are relative to this local machine's endianness.
	if ( GetLocalMachineEndianness() == m_packerEndianness )
	{
		WriteForwardAlongBuffer( &data, dataSize );
	}
	else
	{
		WriteBackwardAlongBuffer( &data, dataSize );
	}

	return true;
}


//--------------------------------------------------------------------------------------------------------------
template <> inline bool BytePacker::Write<Vector2f>( const Vector2f& data )
{
	bool success = Write<float>( data.x );
	if ( !success )
		return false;

	success = Write<float>( data.y );
	return success;
}


//--------------------------------------------------------------------------------------------------------------
template <typename DataType> ByteBufferBookmark BytePacker::ReserveForReading( DataType* out_data ) const
{
	ByteBufferBookmark marker = (ByteBufferBookmark)m_ioOffset;

	Read<DataType>( out_data ); //Needs to Read something because it has to advance the offset by that much.
		//But we can't do a Write, or we lose the data that MIGHT be at this offset.

	return marker; //So we can write over that data later.
}


//--------------------------------------------------------------------------------------------------------------
template <typename DataType> bool BytePacker::ReadAtBookmark( ByteBufferBookmark offset, DataType* out_data ) const
{
	ByteBufferBookmark offsetBeforeRewind = (ByteBufferBookmark)m_ioOffset;

	ResetOffset( offset );
	bool success = Read<DataType>( out_data );

	ResetOffset( offsetBeforeRewind );

	return success;
}


//--------------------------------------------------------------------------------------------------------------
template <typename DataType> bool BytePacker::Read( DataType* out_data ) const
{
	size_t dataSize = sizeof( DataType );

	if ( ( m_ioOffset + dataSize ) > m_maxReadSize )
		return false; //Ran out of valid written data to read.

	if ( GetLocalMachineEndianness() == m_packerEndianness )
	{
		ReadForwardAlongBuffer( out_data, dataSize );
	}
	else
	{
		ReadBackwardAlongBuffer( out_data, dataSize );
	}

	AdvanceOffset( dataSize );
	return true;
}


//--------------------------------------------------------------------------------------------------------------
template <> inline bool BytePacker::Read<Vector2f>( Vector2f* out_data ) const
{
	bool success = Read<float>( &out_data->x );
	if ( !success )
		return false;

	success = Read<float>( &out_data->y );
	return success;
}
