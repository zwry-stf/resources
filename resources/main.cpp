#include <fstream>
#include <vector>
#include <string>
#include <print>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <format>

#include "include/common.h"


inline static constexpr std::string_view json_path = "files.json";

bool read_json(nlohmann::json& out, bool print) {
	std::ifstream file(json_path.data());
	if (!file) {
		if (print)
			std::println("failed to open '{}' for read", json_path);
		return false;
	}

	std::string contents = std::string(
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>()
	);
	if (!file) {
		if (print)
			std::println("failed to read '{}'", json_path);
		return false;
	}

	file.close();

	try {
		out = nlohmann::json::parse(contents);
	}
	catch (...) {
		if (print)
			std::println("failed to parse '{}' contents", json_path);
		return false;
	}

	return true;
}

int compile() {
	constexpr std::string_view header_name = "resources.h";
	constexpr std::string_view source_name = "resources.cpp";

	nlohmann::json j;
	if (!read_json(j, true)) {
		return 1;
	}

	if (!j.contains("files") ||
		!j["files"].is_array()) {
		std::println("invalid json file, please remove first");
		return 1;
	}

	// read
	struct loaded_file {
		int id;
		std::vector<std::uint8_t> data;
	};
	std::vector<loaded_file> loaded_files;

	int index = 0;
	for (auto& i : j["files"]) {
		if (!i.contains("id") ||
			!i["id"].is_number_integer()) {
			std::println("entry '{}' doesn't contain id", index);
			return 1;
		}

		if (!i.contains("path") ||
			!i["path"].is_string()) {
			std::println("entry '{}' doesn't contain path", index);
			return 1;
		}

		const auto path = i["path"].get<std::string>();
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) {
			std::println("failed to open file '{}' for read", path);
			return 1;
		}

		std::streampos file_size = file.tellg();
		if (file_size == std::streampos(-1)) {
			std::println("failed to get file size of file '{}'", path);
			return 1;
		}

		file.seekg(0, std::ios::beg);

		std::vector<std::uint8_t> data(static_cast<std::size_t>(file_size));
		file.read(reinterpret_cast<char*>(data.data()), data.size());
		if (!file) {
			std::println("failed to read file '{}'", path);
			return 1;
		}

		file.close();

		loaded_files.emplace_back(
			i["id"].get<int>(),
			std::move(data)
		);

		index++;
	}

	if (loaded_files.empty()) {
		std::println("no files added");
		return 1;
	}

	// build data
	std::vector<std::uint8_t> raw_data;
	std::vector<std::uint8_t> file_data;
	resources::global_header global_header;
	global_header.num_files = loaded_files.size();
	raw_data.resize(sizeof(global_header));

	// add files
	for (auto& f : loaded_files) {
		const std::size_t offset = file_data.size();
		file_data.resize(file_data.size() + f.data.size());
		std::memcpy(
			file_data.data() + offset,
			f.data.data(),
			f.data.size()
		);

		resources::file_header header;
		header.id = f.id;
		header.data_offset = offset;
		header.data_size = f.data.size();

		const std::size_t raw_offset = raw_data.size();
		raw_data.resize(raw_data.size() + sizeof(header));
		std::memcpy(
			raw_data.data() + raw_offset,
			&header,
			sizeof(header)
		);
	}

	// add global header
	const std::size_t raw_data_offset = raw_data.size();
	global_header.data_pointer = raw_data_offset;
	std::memcpy(
		raw_data.data(),
		&global_header,
		sizeof(global_header)
	);

	// add file data
	raw_data.resize(raw_data.size() + file_data.size());
	std::memcpy(
		raw_data.data() + raw_data_offset,
		file_data.data(),
		file_data.size()
	);

	// header file
	std::ofstream header_file(header_name.data());
	if (!header_file) {
		std::println("failed to open output file '{}'", header_name);
		return 1;
	}

	constexpr std::string_view header_contents = R"(#pragma once
#include <cstdint>

extern constexpr std::uint8_t g_resources[{}];)";

	std::string formatted_contents = std::format(header_contents, raw_data.size());
	header_file.write(formatted_contents.data(), formatted_contents.size());
	if (!header_file) {
		std::println("failed to write to output file '{}'", header_name);
		return 1;
	}

	header_file.close();

	// source file
	std::ofstream source_file(source_name.data());
	if (!source_file) {
		std::println("failed to open output file '{}'", source_name);
		return 1;
	}

	constexpr std::string_view source_contents = R"(#include <cstdint>

constexpr std::uint8_t g_resources[{}] = {{ )";
	std::string source = std::format(source_contents, raw_data.size());

	source.reserve(source.size() +
		raw_data.size() * 6u /* "0x00, " */
	); 
	// format data to hex array
	for (std::size_t i = 0u; i < raw_data.size(); i++) {
		const bool last = i == raw_data.size() - 1u;

		source += "0x";
		constexpr char hex[] = "0123456789ABCDEF";

		const std::uint8_t b = raw_data[i];
		source += hex[b >> 4];
		source += hex[b & 0x0F];

		if (!last)
			source += ", ";
	}
	source += " };";

	source_file.write(source.data(), source.size());
	if (!source_file) {
		std::println("failed to write to output file '{}'", source_name);
		return 1;
	}

	source_file.flush();
	source_file.close();

	std::println("successfully compiled {} resources", loaded_files.size());

	return 0;
}

int add(int num_files, char** files) {
	// read file first
	nlohmann::json j;
	read_json(j, false); // ignore return value, start with empty json if read fails

	if (j.contains("files") &&
		!j["files"].is_array()) {
		std::println("invalid json file, please remove first");
		return 1;
	}

	int start_id = 0;
	if (j.contains("files")) {
		for (auto& i : j["files"]) {
			if (i.contains("id") &&
				i["id"].is_number_integer() &&
				!i["id"].is_null()) {
				const int id = i["id"].get<int>();
				if (id + 1 > start_id)
					start_id = id + 1;
			}
		}
	}

	for (int i = 0; i < num_files; i++) {
		char* path = files[i];

		if (!std::filesystem::exists(path)) {
			std::println("failed to find file '{}'", path);
			return 1;
		}

		nlohmann::json o;
		o["path"] = path;
		o["id"] = start_id;

		j["files"].push_back(o);
		start_id++;
	}

	std::ofstream file(json_path.data());
	if (!file) {
		std::println("failed to open '{}' for write", json_path);
		return 1;
	}

	std::string data = j.dump(4);

	file.write(data.c_str(), data.size());
	if (!file) {
		std::println("failed to write to '{}'", json_path);
		return 1;
	}

	file.flush();
	file.close();

	return 0;
}

void print_usage() {
	std::println("usage: resources -add <file...> | -compile");
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		print_usage();
		return 1;
	}

	auto arg = argv[1];
	if (strcmp(arg, "-compile") == 0) {
		if (argc != 2) {
			std::println("compile expects no arguments, ignoring...");
		}
		return compile();
	}
	else if (strcmp(arg, "-add") == 0) {
		const int num_files = argc - 2;
		if (num_files <= 0) {
			std::println("no files specified...");
			return -1;
		}

		return add(num_files, &argv[2]);
	}
	else {
		std::println("unknown argument '{}'", arg);
		print_usage();
		return -1;
	}
}