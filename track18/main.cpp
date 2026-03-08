#include <iostream>
#include <string>
#include <string_view>
#include <print>
#include <ranges>
#include <filesystem>
#include <span>
#include <vector>
#include "parseArgs.h"

	

void scanFile(const fs::path& filename) {
	std::println(std::cout, "Processing: {}", filename.string());
}



int main(int argc, char* argv[]) {
	auto args = std::span{ argv,static_cast<size_t>(argc) };
	std::vector<fs::path> inputs = getInputs(args);
	
	for (const auto& filename : inputs) {
		scanFile(filename);
	}

	return 0;
}