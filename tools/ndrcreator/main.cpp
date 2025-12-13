#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <nlohmann/json.hpp>
#include <zlib.h>

using json = nlohmann::json;

constexpr int ALIGNMENT = 16;
constexpr uint32_t TYPE_R_NODE  = 0;
constexpr uint32_t TYPE_R_NUM   = 1;
constexpr uint32_t TYPE_R_UTF8  = 2;
constexpr uint32_t TYPE_R_UTF16 = 3;
constexpr uint32_t TYPE_R_NULL  = 0xFFFF;
constexpr char MAGIC[4] = {'N','D','R',0};
constexpr uint32_t VERSION = 1;

struct NodeInput {
    std::string name;
    std::string type;
    json value;
};

struct FlatEntry {
    uint64_t entryID;
    uint64_t parentID;
    std::string name;
    uint32_t entryType;
    uint32_t flags;
    std::vector<uint8_t> data;
};

struct Header {
    char magic[4];
    uint32_t version;
    uint32_t reserved1;
    uint32_t headerChecksum;
    uint64_t totalSize;
    uint64_t reserved2;
    
    Header() {
        std::memset(this, 0, sizeof(Header));
        std::memcpy(magic, MAGIC, 4);
        version = VERSION;
    }
};

struct RegTableRow {
    uint64_t entryID;
    uint64_t entryOffset;
    uint64_t rootID;
    uint64_t reserved;
};

std::vector<uint8_t> padBytes(const std::vector<uint8_t>& b) {
    size_t pad = (ALIGNMENT - (b.size() % ALIGNMENT)) % ALIGNMENT;
    std::vector<uint8_t> res = b;
    res.insert(res.end(), pad, 0);
    return res;
}

uint64_t fnv1aHash64(const std::string& name, uint64_t parentID) {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : name) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    for (size_t i = 0; i < 8; i++) {
        hash ^= (parentID >> (i*8)) & 0xFF;
        hash *= 1099511628211ULL;
    }
    return hash;
}

void writeUint32At(std::vector<uint8_t>& buf, size_t off, uint32_t v) {
    std::memcpy(buf.data() + off, &v, sizeof(v));
}

void writeUint64At(std::vector<uint8_t>& buf, size_t off, uint64_t v) {
    std::memcpy(buf.data() + off, &v, sizeof(v));
}

void parseNodeRecursive(const NodeInput& n, uint64_t parentID, std::vector<FlatEntry>& out) {
    uint64_t myID = fnv1aHash64(n.name, parentID);
    FlatEntry entry;
    entry.entryID = myID;
    entry.parentID = parentID;
    entry.name = n.name;
    entry.flags = 0;

    if (n.type == "R_NODE") {
        entry.entryType = TYPE_R_NODE;
        entry.data.clear();
        out.push_back(entry);
        if (n.value.is_array()) {
            for (auto& childJson : n.value) {
                NodeInput child{childJson["name"], childJson["type"], childJson["value"]};
                parseNodeRecursive(child, myID, out);
            }
        }
    } else if (n.type == "R_U64") {
        entry.entryType = TYPE_R_NUM;
        uint64_t num = n.value.is_number() ? n.value.get<uint64_t>() : 0;
        for (int i = 0; i < 8; i++) {
            entry.data.push_back((num >> (i * 8)) & 0xFF);
        }
        out.push_back(entry);
    } else if (n.type == "R_STR8" || n.type == "R_UTF8") {
        entry.entryType = TYPE_R_UTF8;
        if (n.value.is_string()) {
            std::string s = n.value.get<std::string>();
            entry.data.assign(s.begin(), s.end());
        } else {
            entry.data.clear();
        }
        out.push_back(entry);
    } else if (n.type == "R_STR16" || n.type == "R_UTF16") {
        entry.entryType = TYPE_R_UTF16;
        if (n.value.is_string()) {
            std::string s = n.value.get<std::string>();
            entry.data.clear();
            for (char c : s) {
                uint16_t ch = static_cast<uint8_t>(c);
                entry.data.push_back(ch & 0xFF);
                entry.data.push_back((ch >> 8) & 0xFF);
            }
        } else {
            entry.data.clear();
        }
        out.push_back(entry);
    } else {
        entry.entryType = TYPE_R_NULL;
        entry.data.clear();
        out.push_back(entry);
    }
}

std::vector<FlatEntry> flattenJSON(const std::vector<NodeInput>& nodes) {
    std::vector<FlatEntry> out;
    for (auto& n : nodes) {
        parseNodeRecursive(n, 0, out);
    }
    return out;
}

std::vector<uint8_t> serializeEntry(const FlatEntry& fe) {
    std::vector<uint8_t> buf;

    uint64_t totalSize = 8;
    std::vector<uint8_t> rest;

    uint64_t entryID = fe.entryID;
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&entryID), 
                reinterpret_cast<const uint8_t*>(&entryID) + 8);

    uint64_t nameLen = fe.name.size();
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&nameLen),
                reinterpret_cast<const uint8_t*>(&nameLen) + 8);

    uint64_t reserved = 0;
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&reserved),
                reinterpret_cast<const uint8_t*>(&reserved) + 8);

    rest.insert(rest.end(), fe.name.begin(), fe.name.end());

    uint32_t entryType = fe.entryType;
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&entryType),
                reinterpret_cast<const uint8_t*>(&entryType) + 4);

    uint32_t reserved32 = 0;
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&reserved32),
                reinterpret_cast<const uint8_t*>(&reserved32) + 4);

    uint32_t flags = fe.flags;
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&flags),
                reinterpret_cast<const uint8_t*>(&flags) + 4);

    if (!fe.data.empty()) {
        rest.insert(rest.end(), fe.data.begin(), fe.data.end());
    }

    totalSize += rest.size();

    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&totalSize),
               reinterpret_cast<const uint8_t*>(&totalSize) + 8);
    buf.insert(buf.end(), rest.begin(), rest.end());

    return padBytes(buf);
}

void writeNDR(const std::vector<FlatEntry>& entries, const std::string& outPath) {
    std::vector<std::vector<uint8_t>> serialized;
    for (auto& e : entries) {
        serialized.push_back(serializeEntry(e));
    }

    size_t headerSize = sizeof(Header);
    size_t regBase = 16;
    size_t regRow = sizeof(RegTableRow);
    size_t regTableSize = regBase + entries.size() * regRow;
    regTableSize = ((regTableSize + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;

    size_t startOffset = headerSize + regTableSize;
    std::vector<uint64_t> offsets(entries.size());
    size_t cur = startOffset;
    for (size_t i = 0; i < entries.size(); i++) {
        offsets[i] = cur;
        cur += serialized[i].size();
    }
    size_t totalSize = ((cur + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;

    Header h;
    h.totalSize = totalSize;

    h.headerChecksum = 0;
    std::vector<uint8_t> headerBuf(sizeof(Header));
    std::memcpy(headerBuf.data(), &h, sizeof(Header));
    uint32_t crcHeader = crc32(0L, headerBuf.data(), headerBuf.size());
    h.headerChecksum = crcHeader;
    std::memcpy(headerBuf.data(), &h, sizeof(Header));

    std::vector<uint8_t> tableBuf;
    
    uint64_t numEntries = entries.size();
    tableBuf.insert(tableBuf.end(), reinterpret_cast<uint8_t*>(&numEntries),
                    reinterpret_cast<uint8_t*>(&numEntries) + 8);
    
    uint32_t res = 0;
    tableBuf.insert(tableBuf.end(), reinterpret_cast<uint8_t*>(&res),
                    reinterpret_cast<uint8_t*>(&res) + 4);
    
    uint32_t crcPlaceholder = 0;
    tableBuf.insert(tableBuf.end(), reinterpret_cast<uint8_t*>(&crcPlaceholder),
                    reinterpret_cast<uint8_t*>(&crcPlaceholder) + 4);

    for (size_t i = 0; i < entries.size(); i++) {
        RegTableRow row{entries[i].entryID, offsets[i], entries[i].parentID, 0};
        tableBuf.insert(tableBuf.end(), reinterpret_cast<uint8_t*>(&row),
                        reinterpret_cast<uint8_t*>(&row) + sizeof(row));
    }

    tableBuf = padBytes(tableBuf);
    
    std::memcpy(tableBuf.data() + 12, &crcPlaceholder, 4);
    uint32_t tableCRC = crc32(0L, tableBuf.data(), tableBuf.size());
    std::memcpy(tableBuf.data() + 12, &tableCRC, 4);

    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Cannot open output file: " + outPath);
    }
    
    ofs.write(reinterpret_cast<char*>(headerBuf.data()), headerBuf.size());
    ofs.write(reinterpret_cast<char*>(tableBuf.data()), tableBuf.size());
    for (auto& s : serialized) {
        ofs.write(reinterpret_cast<char*>(s.data()), s.size());
    }
    ofs.close();
}

json decodeNDR(const std::string& inPath) {
    std::ifstream ifs(inPath, std::ios::binary | std::ios::ate);
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + inPath);
    }
    
    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> f(size);
    if (!ifs.read(reinterpret_cast<char*>(f.data()), size)) {
        throw std::runtime_error("Cannot read file: " + inPath);
    }
    ifs.close();

    if (f.size() < sizeof(Header)) {
        throw std::runtime_error("File too small");
    }
    
    Header h;
    std::memcpy(&h, f.data(), sizeof(Header));
    if (std::memcmp(h.magic, MAGIC, 4) != 0) {
        throw std::runtime_error("Bad magic number");
    }

    uint32_t savedChecksum = h.headerChecksum;
    h.headerChecksum = 0;
    std::vector<uint8_t> headerBuf(sizeof(Header));
    std::memcpy(headerBuf.data(), &h, sizeof(Header));
    uint32_t computedChecksum = crc32(0L, headerBuf.data(), headerBuf.size());
    h.headerChecksum = savedChecksum;
    
    if (computedChecksum != savedChecksum) {
        throw std::runtime_error("Header checksum mismatch");
    }

    size_t pos = sizeof(Header);
    if (pos + 16 > f.size()) {
        throw std::runtime_error("File truncated at registration table");
    }
    
    uint64_t numEntries;
    std::memcpy(&numEntries, f.data() + pos, 8);
    pos += 8;
    
    pos += 8;
    
    uint32_t tableCRC;
    std::memcpy(&tableCRC, f.data() + pos - 4, 4);
    
    std::vector<uint8_t> tableCheck(f.data() + sizeof(Header), f.data() + pos + numEntries * sizeof(RegTableRow));
    std::memset(tableCheck.data() + 12, 0, 4);
    uint32_t computedTableCRC = crc32(0L, tableCheck.data(), tableCheck.size());
    
    if (computedTableCRC != tableCRC) {
        throw std::runtime_error("Table checksum mismatch");
    }

    std::map<uint64_t, FlatEntry> entries;
    for (uint64_t i = 0; i < numEntries; i++) {
        if (pos + sizeof(RegTableRow) > f.size()) {
            throw std::runtime_error("File truncated at registration table row");
        }
        
        RegTableRow row;
        std::memcpy(&row, f.data() + pos, sizeof(RegTableRow));
        pos += sizeof(RegTableRow);

        if (row.entryOffset >= f.size()) {
            throw std::runtime_error("Entry offset out of bounds");
        }
        
        size_t entryPos = row.entryOffset;
        if (entryPos + 8 > f.size()) {
            throw std::runtime_error("File truncated at entry size");
        }
        
        uint64_t entrySize;
        std::memcpy(&entrySize, f.data() + entryPos, 8);
        entryPos += 8;

        if (row.entryOffset + entrySize > f.size()) {
            throw std::runtime_error("Entry extends beyond file");
        }
        
        FlatEntry fe;
        std::memcpy(&fe.entryID, f.data() + entryPos, 8);
        entryPos += 8;
        
        uint64_t nameLen;
        std::memcpy(&nameLen, f.data() + entryPos, 8);
        entryPos += 8;
        
        entryPos += 8;
        
        if (entryPos + nameLen > f.size()) {
            throw std::runtime_error("File truncated at entry name");
        }
        
        fe.name.assign(reinterpret_cast<const char*>(f.data() + entryPos), nameLen);
        entryPos += nameLen;
        
        std::memcpy(&fe.entryType, f.data() + entryPos, 4);
        entryPos += 4;
        
        entryPos += 4;
        
        std::memcpy(&fe.flags, f.data() + entryPos, 4);
        entryPos += 4;
        
        size_t bytesRead = entryPos - row.entryOffset;
        if (entrySize < bytesRead) {
            throw std::runtime_error("Invalid entry size");
        }
        
        size_t dataSize = entrySize - bytesRead;
        if (entryPos + dataSize > f.size()) {
            throw std::runtime_error("File truncated at entry data");
        }
        
        if (fe.entryType == TYPE_R_NUM) {
            dataSize = 8; 
        }
        
        if (entryPos + dataSize > f.size()) {
            dataSize = f.size() - entryPos;
        }
        
        fe.data.assign(f.begin() + entryPos, f.begin() + entryPos + dataSize);
        fe.parentID = row.rootID;
        entries[fe.entryID] = fe;
    }

    std::map<uint64_t, std::vector<FlatEntry>> children;
    std::vector<FlatEntry> roots;
    for (auto& [id, e] : entries) {
        if (e.parentID == 0) {
            roots.push_back(e);
        } else {
            children[e.parentID].push_back(e);
        }
    }

    std::function<json(const FlatEntry&)> build;
    build = [&](const FlatEntry& fe) -> json {
        json j;
        j["name"] = fe.name;
        
        switch (fe.entryType) {
            case TYPE_R_NODE:
                j["type"] = "R_NODE";
                j["value"] = json::array();
                for (auto& c : children[fe.entryID]) {
                    j["value"].push_back(build(c));
                }
                break;
            case TYPE_R_NUM:
                j["type"] = "R_U64";
                if (fe.data.size() >= 8) {
                    uint64_t num;
                    std::memcpy(&num, fe.data.data(), 8);
                    j["value"] = num;
                } else {
                    j["value"] = 0;
                }
                break;
            case TYPE_R_UTF8:
                j["type"] = "R_STR8";
                j["value"] = std::string(fe.data.begin(), fe.data.end());
                break;
            case TYPE_R_UTF16:
                j["type"] = "R_STR16";
                {
                    std::u16string u16str;
                    for (size_t i = 0; i + 1 < fe.data.size(); i += 2) {
                        uint16_t c = fe.data[i] | (fe.data[i + 1] << 8);
                        u16str.push_back(c);
                    }
                    std::string s(u16str.begin(), u16str.end());
                    j["value"] = s;
                }
                break;
            default:
                j["type"] = "R_NULL";
                j["value"] = nullptr;
        }
        return j;
    };

    json outArr = json::array();
    for (auto& r : roots) {
        outArr.push_back(build(r));
    }
    return outArr;
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cout << "Usage: ndrcreator [encode|decode]\n";
            return 1;
        }

        if (std::string(argv[1]) == "encode") {
            std::ifstream ifs("data.json");
            if (!ifs) {
                std::cerr << "Cannot open data.json\n";
                return 1;
            }
            json j = json::parse(ifs);
            ifs.close();

            std::vector<NodeInput> nodes;
            for (auto& nj : j) {
                nodes.push_back(NodeInput{nj["name"], nj["type"], nj["value"]});
            }

            auto flat = flattenJSON(nodes);
            writeNDR(flat, "output.ndr");
            std::cout << "Saved: output.ndr\n";
        } else if (std::string(argv[1]) == "decode") {
            json j = decodeNDR("output.ndr");
            std::ofstream ofs("output.decoded.json");
            ofs << j.dump(2);
            ofs.close();
            std::cout << "Decoded into output.decoded.json\n";
        } else {
            std::cout << "Unknown command. Usage: encode|decode\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}