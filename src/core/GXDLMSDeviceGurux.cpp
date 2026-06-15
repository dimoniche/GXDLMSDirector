#include "GXDLMSDevice.h"
#include "GXDLMSCommunicator.h"

#include <GXDLMSObjectCollection.h>

CGXDLMSObjectCollection &GXDLMSDevice::objects()
{
    return m_communicator->client()->GetObjects();
}

GXDLMSCommunicator *GXDLMSDevice::communicator()
{
    return m_communicator.get();
}
