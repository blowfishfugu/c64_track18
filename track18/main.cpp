#include <iostream>
#include <string>
#include <string_view>
#include <print>
#include <ranges>
#include <filesystem>
#include <span>
#include <vector>
#include "parseArgs.h"

	

void scanFile(std::uintmax_t filesize, const fs::path& filename) {
	std::println(std::cout, "Processing: {} ({})", filename.string(),filesize);
}



int main(int argc, char* argv[]) {
	auto args = std::span{ argv,static_cast<size_t>(argc) };
	auto inputs = getInputs<std::tuple<std::uintmax_t,fs::path>>(args);
	
	for (const auto& [filesize,filename] : inputs) {
		scanFile(filesize,filename);
	}

	return 0;
}