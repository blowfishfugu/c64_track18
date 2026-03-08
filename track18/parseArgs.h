#pragma once
#include <vector>
#include <filesystem>
#include <list>

namespace fs = std::filesystem;

auto isD64 = [](const fs::path& filename)->bool {
	std::error_code ec{};
	if (!fs::is_regular_file(filename, ec)) { return false; }
	if (ec) { return false; }

	std::string ext = filename.extension().string();
	if (ext == ".d64" || ext == ".D64") { return true; }
	return false;
	};

//todo: dies als coro?
std::vector<fs::path> getInputs(auto&& args) {
	std::vector<fs::path> inputs;
	std::list<fs::path> subDirs;
	std::error_code ec{};
	for (size_t i{}; const fs::path p : args) {
		++i;
		if (i == 1) { continue; }

		fs::path resolved = fs::absolute(p, ec);
		if (ec) { continue; }

		if (!fs::exists(resolved, ec)) { continue; }

		if (fs::is_directory(resolved, ec)) {
			subDirs.emplace_back(resolved);
		}
		else if (isD64(resolved)) {
			inputs.emplace_back(resolved); //co_yield
		}
	}

	while (!subDirs.empty()) {
		fs::path resolved = subDirs.front();
		subDirs.pop_front();
		if (fs::is_directory(resolved, ec))
		{
			for (auto&& entry : fs::directory_iterator(resolved)) {
				fs::path pathEntry = entry.path();
				if (fs::is_directory(pathEntry)) {
					subDirs.emplace_back(pathEntry);
				}
				else if (isD64(pathEntry)) {
					inputs.emplace_back(pathEntry); //co_yield
				}
			}
		}
	}

	return inputs; //co_return
}