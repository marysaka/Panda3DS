#pragma once
#include <array>
#include <vector>
#include "io_file.hpp"
#include "helpers.hpp"

struct NCCH {
    struct FSInfo { // Info on the ExeFS/RomFS
        u64 offset = 0;
        u64 size = 0;
        u64 hashRegionSize = 0;
    };

    // Descriptions for .text, .data and .rodata sections
    struct CodeSetInfo {
        u32 address = 0;
        u32 pageCount = 0;
        u32 size = 0;

        // Extract the code set info from the relevant header data
        void extract(const u8* headerEntry) {
            address = *(u32*)&headerEntry[0];
            pageCount = *(u32*)&headerEntry[4];
            size = *(u32*)&headerEntry[8];
        }
    };

    u64 partitionIndex = 0;
    u64 fileOffset = 0;

    bool isNew3DS = false;
    bool initialized = false;
    bool compressCode = false; // Shows whether the .code file in the ExeFS is compressed
    bool mountRomFS = false;
    bool encrypted = false;
    bool fixedCryptoKey = false;

    static constexpr u64 mediaUnit = 0x200;
    u64 size = 0; // Size of NCCH converted to bytes
    u32 stackSize = 0;
    u32 bssSize = 0;
    u32 exheaderSize = 0;

    FSInfo exeFS;
    FSInfo romFS;
    CodeSetInfo text, data, rodata;

    // Contents of the .code file in the ExeFS
    std::vector<u8> codeFile;
    // Contains of the cart's save data
    std::vector<u8> saveData;

    // Header: 0x200 + 0x800 byte NCCH header + exheadr
    // Returns true on success, false on failure
    // Partition index/offset/size must have been set before this
    bool loadFromHeader(u8* header, IOFile& file);

    bool hasExtendedHeader() { return exheaderSize != 0; }
    bool hasExeFS() { return exeFS.size != 0; }
    bool hasRomFS() { return romFS.size != 0; }
    bool hasCode() { return codeFile.size() != 0; }
    bool hasSaveData() { return saveData.size() != 0; }

private:
    std::array<u8, 16> primaryKey = {}; // For exheader, ExeFS header and icons
    std::array<u8, 16> secondaryKey = {}; // For RomFS and some files in ExeFS
};