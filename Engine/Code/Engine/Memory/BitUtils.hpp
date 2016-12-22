#pragma once 

#define IS_BITFIELD_SET_AT_INDEX( bitfield, bitIndex ) ( GET_BIT_AT_BITFIELD_INDEX_MASKED( bitfield, bitIndex ) != 0 )
#define SET_BIT( bitfield, bitIndex ) ( bitfield |= GET_BITFIELD_INDEX_MASK( bitIndex ) )
#define GET_BITFIELD_INDEX_MASK(i) (1 << (i)) //2^x, or the xth bit.
#define GET_BIT_AT_BITFIELD_INDEX_MASKED( bitfield, bitIndex ) ( bitfield & GET_BITFIELD_INDEX_MASK( bitIndex ) ) //Shifts for you.
#define GET_BIT_AT_BITFIELD_WITHOUT_INDEX_MASKED( bitfield, bitValueNotAnIndex ) ( bitfield & bitValueNotAnIndex ) //Does not shift.

static const int NUM_BITS_IN_BYTE = 8; //For removing magic numbers.