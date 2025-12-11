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
        entry.data.resize(8);
        std::memcpy(entry.data.data(), &num, 8);
        out.push_back(entry);
    } else if (n.type == "R_STR8" || n.type == "R_UTF8") {
        entry.entryType = TYPE_R_UTF8;
        if (n.value.is_string()) {
            std::string s = n.value.get<std::string>();
            entry.data.assign(s.begin(), s.end());
        }
        out.push_back(entry);
    } else if (n.type == "R_UTF16") {
        entry.entryType = TYPE_R_UTF16;
        if (n.value.is_string()) {
            std::string s = n.value.get<std::string>();
            for (char c : s) {
                uint16_t ch = c;
                entry.data.push_back(ch & 0xFF);
                entry.data.push_back((ch >> 8) & 0xFF);
            }
        }
        out.push_back(entry);
    } else {
        entry.entryType = TYPE_R_NULL;
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

    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&fe.entryID), reinterpret_cast<const uint8_t*>(&fe.entryID)+8);

    uint64_t nameLen = fe.name.size();
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&nameLen), reinterpret_cast<const uint8_t*>(&nameLen)+8);

    uint64_t reserved = 0;
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&reserved), reinterpret_cast<const uint8_t*>(&reserved)+8);

    rest.insert(rest.end(), fe.name.begin(), fe.name.end());

    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&fe.entryType), reinterpret_cast<const uint8_t*>(&fe.entryType)+4);

    uint32_t reserved32 = 0;
    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&reserved32), reinterpret_cast<const uint8_t*>(&reserved32)+4);

    rest.insert(rest.end(), reinterpret_cast<const uint8_t*>(&fe.flags), reinterpret_cast<const uint8_t*>(&fe.flags)+4);

    if (!fe.data.empty()) {
        rest.insert(rest.end(), fe.data.begin(), fe.data.end());
    }

    totalSize += rest.size();

    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&totalSize), reinterpret_cast<const uint8_t*>(&totalSize)+8);
    buf.insert(buf.end(), rest.begin(), rest.end());

    return padBytes(buf);
}

void writeNDR(const std::vector<FlatEntry>& entries, const std::string& outPath) {
    std::vector<std::vector<uint8_t>> serialized;
    for (auto& e : entries) serialized.push_back(serializeEntry(e));

    size_t headerSize = 32;
    size_t regBase = 16;
    size_t regRow = 32;
    size_t regTableSize = regBase + entries.size()*regRow;
    regTableSize = ((regTableSize + ALIGNMENT-1)/ALIGNMENT)*ALIGNMENT;

    size_t startOffset = headerSize + regTableSize;
    std::vector<uint64_t> offsets(entries.size());
    size_t cur = startOffset;
    for (size_t i=0;i<entries.size();i++){
        offsets[i] = cur;
        cur += serialized[i].size();
    }
    size_t totalSize = ((cur + ALIGNMENT-1)/ALIGNMENT)*ALIGNMENT;

    Header h{};
    std::memcpy(h.magic, MAGIC, 4);
    h.version = VERSION;
    h.reserved1 = 0;
    h.headerChecksum = 0;
    h.totalSize = totalSize;
    h.reserved2 = 0;

    std::vector<uint8_t> headerBuf(sizeof(Header));
    std::memcpy(headerBuf.data(), &h, sizeof(Header));

    std::vector<uint8_t> tableBuf;
    uint64_t numEntries = entries.size();
    tableBuf.insert(tableBuf.end(), reinterpret_cast<uint8_t*>(&numEntries), reinterpret_cast<uint8_t*>(&numEntries)+8);
    uint32_t res = 0;
    tableBuf.insert(tableBuf.end(), reinterpret_cast<uint8_t*>(&res), reinterpret_cast<uint8_t*>(&res)+4);
    uint32_t crcPlaceholder = 0;
    tableBuf.insert(tableBuf.end(), reinterpret_cast<uint8_t*>(&crcPlaceholder), reinterpret_cast<uint8_t*>(&crcPlaceholder)+4);

    for (size_t i=0;i<entries.size();i++){
        RegTableRow row{entries[i].entryID, offsets[i], entries[i].parentID, 0};
        tableBuf.insert(tableBuf.end(), reinterpret_cast<uint8_t*>(&row), reinterpret_cast<uint8_t*>(&row)+sizeof(row));
    }

    tableBuf = padBytes(tableBuf);
    uint32_t tableCRC = crc32(0L, tableBuf.data(), tableBuf.size());
    writeUint32At(tableBuf, 12, tableCRC);

    std::ofstream ofs(outPath, std::ios::binary);
    ofs.write(reinterpret_cast<char*>(headerBuf.data()), headerBuf.size());
    ofs.write(reinterpret_cast<char*>(tableBuf.data()), tableBuf.size());
    for (auto& s : serialized) ofs.write(reinterpret_cast<char*>(s.data()), s.size());
    ofs.close();
}

json decodeNDR(const std::string& inPath) {
    std::ifstream ifs(inPath, std::ios::binary);
    std::vector<uint8_t> f((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    if (f.size() < 32) throw std::runtime_error("File too small");
    Header h;
    std::memcpy(&h, f.data(), sizeof(Header));
    if (std::memcmp(h.magic, MAGIC, 4)!=0) throw std::runtime_error("Bad magic");

    size_t pos = 32;
    uint64_t numEntries = *reinterpret_cast<uint64_t*>(f.data()+pos);
    pos += 16;

    std::map<uint64_t, FlatEntry> entries;
    for (uint64_t i=0;i<numEntries;i++){
        RegTableRow row;
        std::memcpy(&row, f.data()+pos, sizeof(RegTableRow));
        pos += sizeof(RegTableRow);

        size_t entryPos = row.entryOffset;
        uint64_t entrySize = *reinterpret_cast<uint64_t*>(f.data()+entryPos);
        entryPos += 8;

        FlatEntry fe;
        fe.entryID = *reinterpret_cast<uint64_t*>(f.data()+entryPos); entryPos+=8;
        uint64_t nameLen = *reinterpret_cast<uint64_t*>(f.data()+entryPos); entryPos+=8;
        entryPos+=8;

        fe.name.assign(f.begin()+entryPos, f.begin()+entryPos+nameLen);
        entryPos += nameLen;

        fe.entryType = *reinterpret_cast<uint32_t*>(f.data()+entryPos); entryPos+=4;
        entryPos+=4;
        fe.flags = *reinterpret_cast<uint32_t*>(f.data()+entryPos); entryPos+=4;

        size_t dataSize = entrySize - (8+8+8+8+nameLen+4+4+4);
        fe.data.assign(f.begin()+entryPos, f.begin()+entryPos+dataSize);

        fe.parentID = row.rootID;
        entries[fe.entryID] = fe;
    }

    std::map<uint64_t, std::vector<FlatEntry>> children;
    std::vector<FlatEntry> roots;
    for (auto& [id, e]: entries) {
        if (e.parentID==0) roots.push_back(e);
        else children[e.parentID].push_back(e);
    }

    std::function<json(const FlatEntry&)> build;
    build = [&](const FlatEntry& fe) -> json {
        json j;
        j["name"] = fe.name;
        switch(fe.entryType){
            case TYPE_R_NODE:
                j["type"] = "R_NODE";
                j["value"] = json::array();
                for (auto& c: children[fe.entryID]) j["value"].push_back(build(c));
                break;
            case TYPE_R_NUM:
                j["type"] = "R_U64";
                if(fe.data.size()>=8) j["value"] = std::to_string(*reinterpret_cast<const uint64_t*>(fe.data.data()));
                else j["value"] = "0";
                break;
            case TYPE_R_UTF8:
                j["type"] = "R_STR8";
                j["value"] = std::string(fe.data.begin(), fe.data.end());
                break;
            case TYPE_R_UTF16:
                j["type"] = "R_UTF16";
                {
                    std::u16string u16str;
                    for (size_t i=0;i+1<fe.data.size();i+=2){
                        uint16_t c = fe.data[i] | (fe.data[i+1]<<8);
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
    for (auto& r: roots) outArr.push_back(build(r));
    return outArr;
}

int main(int argc, char** argv) {
    if(argc<2){ std::cout<<"Usage: ndrcreator [encode|decode]\n"; return 1; }

    if(std::string(argv[1])=="encode"){
        std::ifstream ifs("data.json");
        json j = json::parse(ifs);
        ifs.close();

        std::vector<NodeInput> nodes;
        for(auto& nj: j){
            nodes.push_back(NodeInput{nj["name"], nj["type"], nj["value"]});
        }

        auto flat = flattenJSON(nodes);
        writeNDR(flat, "output.ndr");
        std::cout<<"Saved: output.ndr\n";
    } else if(std::string(argv[1])=="decode"){
        json j = decodeNDR("output.ndr");
        std::ofstream ofs("output.decoded.json");
        ofs << j.dump(2);
        ofs.close();
        std::cout<<"Decoded into output.decoded.json\n";
    } else {
        std::cout<<"Unknown command. Usage: encode|decode\n";
    }
}
