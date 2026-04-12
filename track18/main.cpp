#include <iostream>
#include <string>
#include <string_view>
#include <print>
#include <filesystem>
#include <span>
#include <vector>
#include <map>
#include <fstream>
#include <array>
#include "parseArgs.h"

using u8 = std::uint8_t;

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

using sector = std::array<u8, 256>;

struct FileEntry {
	std::array<u8,2> nextDirTS{};
	u8 filetype{};
	std::array<u8,2> fileLocationTS{};
	std::array<u8,16> fileName{};
	std::array<u8,2> relSideSector{};
	u8 relLength{};
	std::array<u8,6> unusedGeosData{};
	std::array<u8,2> filesize{};
};

enum class FileTypes : std::uint8_t {

};

std::string stringifyFileType(const FileEntry& dirInfo) {
	const std::uint8_t& filebits = dirInfo.filetype;
	const std::uint8_t inspect = (~filebits) & 0b0000'0111; //only looking at last3 bits
	if (inspect == 0b0000'0111) { //8'0 DEL
		return "DEL";
	}
	else if (inspect == 0b0000'0110) { //8'1 SEQ
		return "SEQ";
	}
	else if (inspect == 0b0000'0101) { //8'2 PRG
		return "PRG";
	}
	else if (inspect == 0b0000'0100) { //8'3 USR
		return "USR";
	}
	else if (inspect == 0b0000'0011) { //8'4 REL
		return "REL";
	}
	return "";
}

std::string stringifyLockFlags(const FileEntry& dirInfo) {
	std::string specialFlags{};
	const u8& inspect = dirInfo.filetype;
	if ((inspect & 0b0010'0000) == 0b0010'0000) { //SAVE-@
		specialFlags.push_back('@');
	}
	if ((inspect & 0b0100'0000) == 0b0100'0000) { //locked flag
		specialFlags.push_back('>');
	}
	if ((inspect & 0b1000'0000) == 0b0100'0000) { //closed flag
		specialFlags.push_back('*');
	}
	return specialFlags;
}

static_assert(sizeof(FileEntry) == 32);


consteval size_t bytesOf(int from, int to) {
	if (to < from) {
		return static_cast<size_t>(from) - to + 0x01;
	}
	return static_cast<size_t>(to) - from + 0x01;
}

struct BAMTrack {
	u8 FreeSectors{};
	std::array<u8,3> mask{};
};

std::string stringifyBamTrack(const BAMTrack& trackInfo) {
	std::string bits(24ull,'\0'); //<- alternative: use std::bitset
	int sector = 0;
	for (const u8& b : trackInfo.mask) {
		u8 current{ b };
		for( int i=0;i<8;++i){
			bits[sector] = (current & 0x01)+'0';
			++sector;
			current >>= 1;
		}
	}
	
	return std::format("Free: {:0>3}, {}", trackInfo.FreeSectors, bits);
}

struct BAMSector {
	std::array<u8, bytesOf(0x00, 0x01)> nextDirTS{}; //Track/Sector location of the first directory sector (should	be set to 18 / 1 but it doesn't matter, and don't trust  what		is there, always go to 18 / 1 for first directory entry)
	u8 DiskDOSVersionType{}; //Disk DOS version type $41("A"), or $00, anything else leads to "soft write protection" (error code 73).
	u8 unused{};
	std::array<BAMTrack, 35> BAMEntries{}; //BAM entries for each track, in groups  of  four  bytes  per	track, starting on track 1 (see below for more details)
	std::array<u8, bytesOf(0x90, 0x9F)> DiskName{}; //Disk Name (padded with $A0)
	std::array<u8, bytesOf(0xA0, 0xA1)> _padA0{}; //Filled with $A0
	std::array<u8, bytesOf(0xA2, 0xA3)> DiskID{}; //Disk ID
	u8 _padA4{}; //Usually $A0
	std::array<u8, bytesOf(0xA5, 0xA6)> DOSType{}; //DOS type, usually "2A"
	std::array<u8, bytesOf(0xA7, 0xAA)> _padA7{}; //Filled with $A0
	std::array<u8, bytesOf(0xAB, 0xFF)> SpecialBams{}; //Normally unused ($00), except for 40 track extended format, see the following two entries:
	//std::array<u8, bytesOf(0xAC,0xBF)  ; //DOLPHIN DOS track 36-40 BAM entries (only for 40 track)
	//std::array<u8, bytesOf(0xC0,0xD3)  ; //SPEED DOS track 36-40 BAM entries (only for 40 track)
	//std::array<u8, bytesOf(0xD4,0xFF)  ; //always unused
};

static_assert(sizeof(BAMSector) == 256); //sizeof(sector)
constexpr size_t FileEntriesPerSector = (sizeof(sector)) / sizeof(FileEntry);
/// <summary>
/// Track18 has 19 sectors,
/// first is the BAM (BlockAvailablityMap)
/// further 18 sectors are DirectoryEntries, each 32Byte.
/// </summary>
struct Track18 {
	BAMSector BAM{}; //256, Track18, Sector 0
	std::array<FileEntry, 18*FileEntriesPerSector> fileEntries{}; //144 entries
};

template<size_t arraySize>
auto petToAscii(const std::array<u8,arraySize>& byteArray) {
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
			std::println(std::cout, "Track {:0>2}: {}", trackID, stringifyBamTrack(trackInfo) );
			++trackID;
		}

		for (int entryPos = 0;
			const FileEntry& entry : directory.fileEntries) { //Todo: follow T/S-Chain
			
			if ((entry.filetype & u8{ 0x80 }) == u8{ 0x80 })
			{
				auto fileName = petToAscii(entry.fileName);
				auto fileType = stringifyFileType(entry);
				auto fileLocks = stringifyLockFlags(entry);
				std::println( std::cout, "{:0>3x}: ?{:0>2x}/{:0>2x}? \"{:<16}\" {}{}",
					entryPos,
					std::get<0>(entry.nextDirTS),std::get<1>(entry.nextDirTS)
					, fileName, fileLocks, fileType);
			}
			
			++entryPos;
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