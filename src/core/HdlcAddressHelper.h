#pragma once

#include <GXDLMSClient.h>

namespace HdlcAddressHelper {

inline void decodeServerAddress(unsigned long serverAddress, unsigned short &logical,
                                unsigned short &physical)
{
    if (serverAddress < 0x4000) {
        logical = static_cast<unsigned short>(serverAddress >> 7);
        physical = static_cast<unsigned short>(serverAddress & 0x7F);
    } else {
        logical = static_cast<unsigned short>(serverAddress >> 14);
        physical = static_cast<unsigned short>(serverAddress & 0x3FFF);
    }
}

inline unsigned long encodeServerAddress(unsigned short logical, unsigned short physical)
{
    const int address = CGXDLMSClient::GetServerAddress(logical, physical);
    if (address < 0)
        return static_cast<unsigned long>(logical << 14 | physical);
    return static_cast<unsigned long>(address);
}

} // namespace HdlcAddressHelper
