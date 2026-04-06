#include <iostream>
#include <string>
#include <string_view>
#include <print>
#include <ranges>
#include <filesystem>
#include <span>
#include <vector>
#include <map>
#include <fstream>
#include <array>
#include <bitset>
#include "parseArgs.h"

struct DiskType {
	std::uintmax_t size{};
	int tracks{};
	int errorbytes{};
};


const std::map<std::uintmax_t, DiskType> DiskTypes{
	{174848, {174848,35,0}},
	{175531, {175531,35,683}},
	{196608, {196608,40,0}},
	{197376, {197376,40,768}},
};

using sector = std::array<std::byte, 256>;

struct DirEntry {
	std::array<std::byte,2> nextDirLocationTS{};
	std::byte filetype{};
	std::array<std::byte,2> fileLocationTS{};
	std::array<std::byte,16> fileName{};
	std::array<std::byte,2> relSideSector{};
	std::byte relLength{};
	std::array<std::byte,6> unusedGeosData{};
	std::array<std::byte,2> filesize{};
};

static_assert(sizeof(DirEntry) == 32);

consteval size_t bytesOf(int from, int to) {
	return static_cast<size_t>(to) - from + 0x01;
}

struct BAMTrack {
	std::byte FreeSectors{};
	std::array<std::byte,3> mask{};
};

std::string interpretBamTrack(const BAMTrack& trackInfo) {
	std::string bits(24ull,'\0');
	int sector = 0;
	for (const std::byte& b : trackInfo.mask) {
		int current{ (int)b };
		for( int i=0;i<8;++i){
			bits[sector] = (current & 0x01)+'0';
			++sector;
			current >>= 1;
		}
	}
	
	return std::format("Free: {:0>3}, {}", static_cast<int>(trackInfo.FreeSectors), bits);
}

struct BAMSector {
	std::array<std::byte, bytesOf(0x00, 0x01)> nextDirLocationTS{}; //Track/Sector location of the first directory sector (should	be set to 18 / 1 but it doesn't matter, and don't trust  what		is there, always go to 18 / 1 for first directory entry)
	std::byte DiskDOSVersionType{}; //Disk DOS version type $41("A"), or $00, anything else leads to "soft write protection" (error code 73).
	std::byte unused{};
	std::array<BAMTrack, 35> BAMEntries{}; //BAM entries for each track, in groups  of  four  bytes  per	track, starting on track 1 (see below for more details)
	std::array<std::byte, bytesOf(0x90, 0x9F)> DiskName{}; //Disk Name (padded with $A0)
	std::array<std::byte, bytesOf(0xA0, 0xA1)> _padA0{}; //Filled with $A0
	std::array<std::byte, bytesOf(0xA2, 0xA3)> DiskID{}; //Disk ID
	std::byte _padA4{}; //Usually $A0
	std::array<std::byte, bytesOf(0xA5, 0xA6)> DOSType{}; //DOS type, usually "2A"
	std::array<std::byte, bytesOf(0xA7, 0xAA)> _padA7{}; //Filled with $A0
	std::array<std::byte, bytesOf(0xAB, 0xFF)> SpecialBams{}; //Normally unused ($00), except for 40 track extended format, see the following two entries:
	//std::array<std::byte, bytesOf(0xAC,0xBF)  ; //DOLPHIN DOS track 36-40 BAM entries (only for 40 track)
	//std::array<std::byte, bytesOf(0xC0,0xD3)  ; //SPEED DOS track 36-40 BAM entries (only for 40 track)
	//std::array<std::byte, bytesOf(0xD4,0xFF)  ; //always unused
};

static_assert(sizeof(BAMSector) == 256); //sizeof(sector)

/// <summary>
/// Track18 has 19 sectors,
/// first is the BAM (BlockAvailablityMap)
/// further 18 sectors are DirectoryEntries, each 32Byte.
/// </summary>
struct Track18 {
	BAMSector BAM{}; //256
	std::array<DirEntry, 18*(sizeof(sector))/sizeof(DirEntry)> direntries{}; //144 entries
};

template<size_t arraySize>
auto petToAscii(const std::array<std::byte,arraySize>& byteArray) {
	//WIP: currently just removing the padding, might return a string-copy some day?
	auto trim = [](std::string_view& v) {
		while (!v.empty() && v.starts_with(static_cast<char>(0xA0))) { v.remove_prefix(1ull); }
		while (!v.empty() && v.ends_with(static_cast<char>(0xA0))) { v.remove_suffix(1ull); }
		};

	std::string_view fileName{ (const char*)byteArray.data() , (const char*)(byteArray.data() + byteArray.size()) };
	trim(fileName);
	return fileName;
}

void scanFile(std::uintmax_t filesize, const fs::path& filename) {
	std::println(std::cout, "Processing: {} ({})", filename.string(),filesize);
	if (auto foundType = DiskTypes.find(filesize); foundType != DiskTypes.end()) {
		DiskType const& diskType = foundType->second;
		std::println(std::cout, "DiskType: {} tracks, {} errorbytes", diskType.tracks, diskType.errorbytes);
		
		Track18 directory{};
		std::ifstream ifs(filename, std::ios::binary);
		ifs.seekg(0x16500);
		ifs.read((char*)&directory, sizeof(Track18));
		ifs.close();

		auto diskName = petToAscii( directory.BAM.DiskName );
		std::cout << "DISKNAME: " << diskName << "\n";
		std::println(std::cout, "                    :          11111111112222");
		std::println(std::cout, "                    :012345678901234567890123");
		std::println(std::cout, "                    :------------------------");
		for (int trackID = 1; const BAMTrack& trackInfo : directory.BAM.BAMEntries) {
			std::println(std::cout, "Track {:0>2}: {}", trackID, interpretBamTrack(trackInfo) );
			++trackID;
		}

		for (const DirEntry& entry : directory.direntries) {
			/*
			02: File type.
                 Typical values for this location are:
                   $00 - Scratched (deleted file entry)
                    80 - DEL
                    81 - SEQ
                    82 - PRG
                    83 - USR
                    84 - REL
			*/
			if ((entry.filetype & std::byte{ 0x80 }) == std::byte{ 0x80 })
			{
				auto fileName = petToAscii(entry.fileName);
				std::println( std::cout, "\"{:<16}\"", fileName );
			}
		}

		return;
	}
	std::cout << "unknown disksize "<< filesize<< "\n";
	return;
}



int main(int argc, char* argv[]) {
	auto args = std::span{ argv,static_cast<size_t>(argc) };
	auto inputs = getInputs<std::tuple<std::uintmax_t,fs::path>>(args);
	
	for (const auto& [filesize,filename] : inputs) {
		scanFile(filesize,filename);
	}

	return 0;
}