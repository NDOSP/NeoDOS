#include <cstdint>
#include <cstddef>
#include <fstream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr size_t ALIGN = 16;

size_t align_up(size_t offset, size_t align = ALIGN) {
    return (offset + align - 1) & ~(align - 1);
}

uint32_t crc32(const void* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

std::vector<uint8_t> bitmap_to_bytes(const std::vector<std::vector<int>>& bitmap) {
    std::vector<uint8_t> out;
    for (auto& row : bitmap) {
        uint8_t b = 0;
        for (size_t i = 0; i < row.size(); i++) {
            b |= (row[i] & 1) << (7 - i);
        }
        out.push_back(b);
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        throw std::runtime_error("Usage: json_to_nff <input.json> <output.nff>");
    }

    std::ifstream f(argv[1]);
    if (!f) throw std::runtime_error("Cannot open input file");
    json font;
    f >> font;

    uint16_t width = font["width"];
    uint16_t height = font["height"];
    auto& chars = font["chars"];
    uint32_t char_count = chars.size();

    size_t header_size = 16;
    size_t char_entry_size = 16;
    size_t data_offset = align_up(header_size + char_count * char_entry_size);

    std::vector<uint8_t> nff_bytes;
    nff_bytes.insert(nff_bytes.end(), {'N','F','F',0});
    uint32_t version = 1;
    nff_bytes.insert(nff_bytes.end(), reinterpret_cast<uint8_t*>(&version), reinterpret_cast<uint8_t*>(&version) + sizeof(version));
    uint16_t w = width;
    uint16_t h = height;
    uint32_t cc = char_count;
    nff_bytes.insert(nff_bytes.end(), reinterpret_cast<uint8_t*>(&w), reinterpret_cast<uint8_t*>(&w) + sizeof(w));
    nff_bytes.insert(nff_bytes.end(), reinterpret_cast<uint8_t*>(&h), reinterpret_cast<uint8_t*>(&h) + sizeof(h));
    nff_bytes.insert(nff_bytes.end(), reinterpret_cast<uint8_t*>(&cc), reinterpret_cast<uint8_t*>(&cc) + sizeof(cc));
    nff_bytes.resize(align_up(nff_bytes.size()));

    std::vector<uint8_t> char_entries(char_count * char_entry_size, 0);
    nff_bytes.insert(nff_bytes.end(), char_entries.begin(), char_entries.end());

    size_t current_offset = data_offset;
    std::vector<std::vector<uint8_t>> bitmap_datas;
    for (size_t i = 0; i < char_count; i++) {
        auto& c = chars[i];
        auto bitmap_bytes = bitmap_to_bytes(c["bitmap"]);
        uint32_t checksum = crc32(bitmap_bytes.data(), bitmap_bytes.size());

        size_t entry_start = header_size + i * char_entry_size;
        nff_bytes[entry_start + 0] = 0;
        nff_bytes[entry_start + 1] = static_cast<uint8_t>(c["ascii"]);
        nff_bytes[entry_start + 2] = 0;
        nff_bytes[entry_start + 3] = 0;
            
        *reinterpret_cast<uint64_t*>(&nff_bytes[entry_start + 4]) = current_offset;
        *reinterpret_cast<uint32_t*>(&nff_bytes[entry_start + 12]) = checksum;

        nff_bytes.insert(nff_bytes.end(), bitmap_bytes.begin(), bitmap_bytes.end());
        size_t pad_len = align_up(bitmap_bytes.size(), ALIGN) - bitmap_bytes.size();
        nff_bytes.insert(nff_bytes.end(), pad_len, 0);
        current_offset += bitmap_bytes.size() + pad_len;
    }

    std::ofstream out(argv[2], std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output file");
    out.write(reinterpret_cast<char*>(nff_bytes.data()), nff_bytes.size());
    return 0;
}
