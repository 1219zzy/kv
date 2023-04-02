#include "util.h"
#include <stdint.h>

namespace kv {
    namespace util
    {
        uint32_t DecodeFixed32(const char* ptr)
        {
            if (!ptr) return 0;
            uint32_t result;
            result = static_cast<uint32_t>(static_cast<unsigned char>(ptr[0]));
            result |= static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8;
            result |= static_cast<uint32_t>(static_cast<unsigned char>(ptr[2])) << 16;
            result |= static_cast<uint32_t>(static_cast<unsigned char>(ptr[3])) << 24;
            return result;             
        }
    } // namespace util
    
} // namespace kv