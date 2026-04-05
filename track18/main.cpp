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

/// <summary>
/// Track18 has 19 sectors,
/// first is the BAM (BlockAvaliablityMap)
/// further 18 sectors are DirectoryEntries, each 32Byte.
/// </summary>
struct Track18 {
	sector BAM{}; //256
	std::array<DirEntry, 18*(sizeof(sector)/sizeof(DirEntry))> direntries{}; //144 entries
};

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
				auto removePadding=[](std::string_view & v) {
					while (!v.empty() && v.starts_with(0xA0)) { v.remove_prefix(1ull); }
					while (!v.empty() && v.ends_with(0xA0)) { v.remove_suffix(1ull); }
				};

				std::string_view fileName{ (char*)entry.fileName.data() , (char*)entry.fileName.data() + entry.fileName.size() };
				removePadding(fileName);
				std::cout << fileName << "\n";
			}
		}

		return;
	}
	std::println(std::cout, "unknown disksize {}", filesize);
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