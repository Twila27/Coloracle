#include "Engine/Networking/NetConnectionUtils.hpp"
#include "Engine/Networking/NetMessage.hpp"


//--------------------------------------------------------------------------------------------------------------
NetMessage* InOrderChannelData::FindAndRemoveMessageForSequenceID( uint16_t sid )
{
	for ( auto msgIter = outOfOrderReceivedSequencedMessages.begin(); msgIter != outOfOrderReceivedSequencedMessages.end(); ++msgIter )
	{
		NetMessage* msg = *msgIter;
		ASSERT_RECOVERABLE( msg != nullptr, "Unexpected nullptr in FindAndRemoveMessageForSequenceID." );

		if ( msg->GetSequenceID() == sid )
		{
			outOfOrderReceivedSequencedMessages.erase( msgIter );
			return msg;
		}
	}

	return nullptr;
}


//--------------------------------------------------------------------------------------------------------------
bool InOrderMessageComparator::operator()( const NetMessage* a, const NetMessage* b ) const
{
	return UnsignedLessThan( a->GetSequenceID(), b->GetSequenceID() );
}
