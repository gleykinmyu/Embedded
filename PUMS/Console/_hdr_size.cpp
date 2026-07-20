#include <cstdio>
#include <cstddef>
#include <cstdint>
#define _USE_LFN 0
namespace BIF { inline constexpr std::size_t kDirNameSize = 13u; }
namespace smcp { namespace file {
inline constexpr std::size_t kPathPrefixRoom = 16u;
inline constexpr std::size_t kPathSize = BIF::kDirNameSize + kPathPrefixRoom;
struct Header {
    char name[kPathSize]{};
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t content_layers = 0;
    uint16_t section_count = 0;
    uint16_t header_crc16 = 0;
    uint32_t body_crc32 = 0;
    uint32_t total_size = 0;
};
}}
int main() {
  using smcp::file::Header;
  using smcp::file::kPathSize;
  std::printf("kPathSize=%zu sizeof=%zu alignof=%zu offsetof magic=%zu\n",
    kPathSize, sizeof(Header), alignof(Header), offsetof(Header, magic));
}
