#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <sstream>
#include <vector>
#include <bitset>

struct PageTableFlags {
    const char* name;
    uint64_t bit;
    const char* description;
};

std::vector<PageTableFlags> page_flags = {
    {"P",    1ULL << 0,  "Present"},
    {"RW",   1ULL << 1,  "Read/Write"},
    {"US",   1ULL << 2,  "User/Supervisor"},
    {"PWT",  1ULL << 3,  "Write-Through"},
    {"PCD",  1ULL << 4,  "Cache Disable"},
    {"A",    1ULL << 5,  "Accessed"},
    {"D",    1ULL << 6,  "Dirty (or PS for directories)"},
    {"PS",   1ULL << 7,  "Page Size (2MB/1GB page)"},
    {"G",    1ULL << 8,  "Global"},
    {"PAT",  1ULL << 12, "Page Attribute Table"},
    {"NX",   1ULL << 63, "No Execute"},
};

const uint64_t ADDR_MASK_L4 = 0x000FFFFFFFFFF000;
const uint64_t ADDR_MASK_L3 = 0x000FFFFFFFFFF000;
const uint64_t ADDR_MASK_L2 = 0x000FFFFFFFFFF000;
const uint64_t FLAGS_MASK   = 0x0000000000000FFF;  

void print_flags(uint64_t entry) {
    std::cout << "\nFlags:\n";
    for (const auto& flag : page_flags) {
        if (entry & flag.bit) {
            std::cout << "  " << flag.name << " (" << flag.description << ")\n";
        }
    }
}

void analyze_entry(uint64_t entry, int level) {
    std::cout << std::hex << std::setfill('0');
    std::cout << "Page Table Entry: 0x" << std::setw(16) << entry << "\n";
    
    if (!(entry & (1ULL << 0))) {
        std::cout << "  Entry is NOT PRESENT (bit 0 = 0)\n";
        return;
    }
    
    uint64_t phys_addr = 0;
    const char* page_type = "4KB page";
    
    if (level == 4) { 
        phys_addr = entry & ADDR_MASK_L4;
    } else if (level == 3) { 
        if (entry & (1ULL << 7)) {  
            phys_addr = entry & 0x000FFFFFC0000000;
            page_type = "1GB page";
        } else {
            phys_addr = entry & ADDR_MASK_L3;
            page_type = "PDPT (next level table)";
        }
    } else if (level == 2) {
        if (entry & (1ULL << 7)) { 
            phys_addr = entry & 0x000FFFFFFFE00000;  
            page_type = "2MB page";
        } else {
            phys_addr = entry & ADDR_MASK_L2;
            page_type = "PD (next level table)";
        }
    } else if (level == 1) {
        phys_addr = entry & ADDR_MASK_L4;
        page_type = "4KB page";
    }
    
    std::cout << "  Physical Address: 0x" << std::setw(16) << phys_addr << "\n";
    std::cout << "  Page Type: " << page_type << "\n";
    
    print_flags(entry);
    
    std::cout << "\nSpecial Analysis:\n";
    
    if ((phys_addr & 0xFFF) != 0) {
        std::cout << "  WARNING: Physical address is NOT 4KB-aligned! (lower 12 bits = 0x" 
                  << (phys_addr & 0xFFF) << ")\n";
    }
    
    uint64_t reserved_bits = entry & 0xFFE0000000000000; 
    if (reserved_bits) {
        std::cout << "  WARNING: Reserved bits set: 0x" << reserved_bits << "\n";
    }
    
    if ((level == 4 || level == 1) && (entry & (1ULL << 7))) {
        std::cout << "  ERROR: PS bit set at invalid level!\n";
    }
    
    if (entry & (1ULL << 63)) {
        std::cout << "  NX (No Execute) bit is SET - page cannot execute code\n";
    }
}

void interactive_mode() {
    std::cout << "Interactive Page Table Entry Analyzer\n";
    std::cout << "Enter entries in hex (0x...) or 'q' to quit\n\n";
    
    while (true) {
        std::string input;
        std::cout << "Enter level (1=PTE, 2=PDE, 3=PDPTE, 4=PML4E): ";
        std::getline(std::cin, input);
        
        if (input == "q" || input == "Q") break;
        
        int level;
        try {
            level = std::stoi(input);
            if (level < 1 || level > 4) {
                std::cout << "Invalid level (must be 1-4)\n";
                continue;
            }
        } catch (...) {
            std::cout << "Invalid input\n";
            continue;
        }
        
        std::cout << "Enter 64-bit page table entry (hex): ";
        std::getline(std::cin, input);
        
        if (input == "q" || input == "Q") break;
        
        if (input.size() > 2 && input[0] == '0' && input[1] == 'x') {
            input = input.substr(2);
        }
        
        uint64_t entry;
        std::stringstream ss;
        ss << std::hex << input;
        
        if (!(ss >> entry)) {
            std::cout << "Invalid hex value\n";
            continue;
        }
        
        std::cout << "\n=================================\n";
        analyze_entry(entry, level);
        std::cout << "=================================\n\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        interactive_mode();
        return 0;
    }
    
    if (argc != 3) {
        std::cerr << "Usage:\n";
        std::cerr << "  " << argv[0] << " [level] [hex_value]  - Analyze single entry\n";
        std::cerr << "  " << argv[0] << "                      - Interactive mode\n";
        std::cerr << "\nLevels:\n";
        std::cerr << "  1 = PTE (Page Table Entry)\n";
        std::cerr << "  2 = PDE (Page Directory Entry)\n";
        std::cerr << "  3 = PDPTE (Page Directory Pointer Table Entry)\n";
        std::cerr << "  4 = PML4E (Page Map Level 4 Entry)\n";
        return 1;
    }
    
    try {
        int level = std::stoi(argv[1]);
        if (level < 1 || level > 4) {
            std::cerr << "Error: Level must be 1-4\n";
            return 1;
        }
        
        std::string input = argv[2];
        if (input.size() > 2 && input[0] == '0' && input[1] == 'x') {
            input = input.substr(2);
        }
        
        uint64_t entry;
        std::stringstream ss;
        ss << std::hex << input;
        
        if (!(ss >> entry)) {
            std::cerr << "Error: Invalid hex value\n";
            return 1;
        }
        
        analyze_entry(entry, level);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}