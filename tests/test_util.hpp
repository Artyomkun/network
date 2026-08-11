#pragma once

// Helper functions for tests.

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "test_framework.hpp"

namespace testutil {

// Path to a fresh temporary file; the file is removed before creation.
inline std::string tempPath(const std::string& tag) {
    static int counter = 0;
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("logger_test_" + std::to_string(++counter) + "_" + tag);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return path.string();
}

// Reads all lines of a file.
inline std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Returns the number of lines in a file.
inline std::size_t countLines(const std::string& path) {
    return readLines(path).size();
}

}  // namespace testutil