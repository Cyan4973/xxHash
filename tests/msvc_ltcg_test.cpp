// Reproduces https://github.com/Cyan4973/xxHash/issues/1038.
#include "../xxhash.h"

#include <cstdint>

struct AccessTimesHeader
{
    std::uint32_t magic = 0x7363617aU;
    std::uint32_t version = 1;
    std::uint32_t accessTimeCount = 0;
    std::uint32_t checksum = 0;
};

static_assert(sizeof(AccessTimesHeader) == 16, "unexpected padding");

int main()
{
    AccessTimesHeader const header;
    XXH32_hash_t const checksum = XXH32(&header.magic,
                                        sizeof(header) - sizeof(header.checksum),
                                        0xC0C0BABAU);
    return checksum != 0xA040CDB3U;
}
