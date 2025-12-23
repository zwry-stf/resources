#include <fstream>
#include <vector>
#include <string>
#include <print>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <format>
#include <assert.h>

#include "include/resources/common.h"


inline static constexpr std::string_view json_path = "files.json";
inline static constexpr std::string_view header_path = "resource_data.h";
inline static constexpr std::string_view source_path = "resource_data.cpp";
inline static constexpr std::string_view default_resources_path = "resources.h";
inline static constexpr std::string_view default_resources_namespace = "resources";

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

bool write_json(nlohmann::json& contents) {
	std::ofstream file(json_path.data());
	if (!file) {
		std::println("failed to open '{}' for write", json_path);
		return false;
	}

	if (!contents.contains("resources_path") ||
		!contents["resources_path"].is_string()) {
		contents["resources_path"] = default_resources_path;
	}

	std::string data = contents.dump(4);

	file.write(data.c_str(), data.size());
	if (!file) {
		std::println("failed to write to '{}'", json_path);
		return false;
	}

	file.flush();
	file.close();

	return true;
}

int compile() {
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
		std::string name;
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

		if (!i.contains("name") ||
			!i["name"].is_string()) {
			std::println("entry '{}' doesn't contain name", index);
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
			i["name"].get<std::string>(),
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
	std::ofstream header_file(header_path.data());
	if (!header_file) {
		std::println("failed to open output file '{}'", header_path);
		return 1;
	}

	constexpr std::string_view header_contents = R"(#pragma once
#include <cstdint>

extern const std::uint8_t g_resources[{}];)";

	std::string formatted_contents = std::format(header_contents, raw_data.size());
	header_file.write(formatted_contents.data(), formatted_contents.size());
	if (!header_file) {
		std::println("failed to write to output file '{}'", header_path);
		return 1;
	}

	header_file.close();

	// source file
	std::ofstream source_file(source_path.data());
	if (!source_file) {
		std::println("failed to open output file '{}'", source_path);
		return 1;
	}

	constexpr std::string_view source_contents = R"(#include <cstdint>

const std::uint8_t g_resources[{}] = {{ )";
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
		std::println("failed to write to output file '{}'", source_path);
		return 1;
	}

	source_file.flush();
	source_file.close();

	// build resources file (ids)
	std::string resources_path = default_resources_path.data();
	if (j.contains("resources_path") &&
		j["resources_path"].is_string()) {
		resources_path = j["resources_path"].get<std::string>();
	}

	std::ofstream out_resources(resources_path);
	if (!out_resources) {
		std::println("failed to open output file '{}'", resources_path);
		return 1;
	}

	std::string resources_namespace = default_resources_namespace.data();
	if (j.contains("resources_namespace") &&
		j["resources_namespace"].is_string()) {
		resources_namespace = j["resources_namespace"].get<std::string>();
	}

	constexpr std::string_view resources_contents = R"(#pragma once

namespace {} {{
)";

	std::string resources_data = std::format(resources_contents, resources_namespace);
	for (auto& f : loaded_files) {
		resources_data += std::format("    inline constexpr int {} = {};\n", f.name, f.id);
	}

	resources_data += "}";

	out_resources.write(resources_data.data(), resources_data.size());
	if (!out_resources) {
		std::println("failed to write to output file '{}'", resources_path);
		return 1;
	}

	out_resources.flush();
	out_resources.close();

	std::println("successfully compiled {} resources", loaded_files.size());

	return 0;
}

int set_option(int num_args, char** args) {
	assert(num_args == 2);
	(void)num_args;

	auto* option = args[0];
	auto* value = args[1];
	if (strlen(value) == 0u) {
		std::println("value is empty");
		return -1;
	}

	if (strcmp(option, "resources_path") == 0) {

		nlohmann::json j;
		read_json(j, false);

		j["resources_path"] = value;

		if (!write_json(j))
			return 1;
	}
	else if (strcmp(option, "resources_namespace") == 0) {
		nlohmann::json j;
		read_json(j, false);

		j["resources_namespace"] = value;

		if (!write_json(j))
			return 1;
	}
	else {
		std::println("invalid option");
		std::println("supported options: resources_path|resources_namespace");
		return -1;
	}

	std::println("successfully set '{}' for option '{}'", value, option);

	return 0;
}

std::string generate_name(std::string path) {
	auto pos = path.find_last_of("/");
	if (pos != std::string::npos)
		path = path.substr(pos + 1u);

	// replace unusable characters
	for (auto& c : path) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
			c = '_';
		}
	}

	// first character can't be a digit
	if (std::isdigit(static_cast<unsigned char>(path[0]))) { 
		path.insert(path.begin(), '_');
	}

	return path;
}

int add(int num_files, char** files) {
	assert(num_files > 0);

	// read file first
	nlohmann::json j;
	read_json(j, false); // ignore return value, start with empty json if read fails

	int start_id = 0;
	if (j.contains("files") &&
		j["files"].is_array()) {
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
	else {
		j["files"] = nlohmann::json::array();
	}

	int added_files = 0;
	for (int i = 0; i < num_files; i++) {
		std::string path = files[i];

		if (!std::filesystem::exists(path)) {
			std::println("failed to find file '{}'", path);
			return 1;
		}

		for (auto& f : j["files"]) {
			if (f.contains("path") &&
				f["path"].is_string()) {
				if (f["path"].get<std::string>() == path) {
					std::println("'{}' already added, ignoring...", path);
					return 1;
				}
			}
		}

		std::string generated_name = generate_name(path);

		int modified_added = 0;
		while (true) {
			bool found = false;

			std::string name = generated_name;
			if (modified_added != 0)
				name += std::to_string(modified_added);

			for (auto& f : j["files"]) {
				if (f.contains("name") &&
					f["name"].is_string()) {
					if (f["name"].get<std::string>() == name) {
						found = true;
						break;
					}
				}
			}
			if (!found) {
				generated_name = name;
				break;
			}

			modified_added++;
		}

		nlohmann::json o;
		o["path"] = path;
		o["id"] = start_id;
		o["name"] = generated_name;

		j["files"].push_back(o);
		start_id++;
		added_files++;
	}

	if (!write_json(j))
		return 1;

	std::println("successfully added {} files", added_files);

	return 0;
}

void print_usage() {
	std::println("usage: resources -add <file...> | -compile | -set_option <option> <value>");
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
	else if (strcmp(arg, "-set_option") == 0) {
		const int num_args = argc - 2;
		if (num_args != 2) {
			std::println("unknown arguments '{}'", arg);
			print_usage();
			return -1;
		}

		return set_option(num_args, &argv[2]);
	}
	else {
		std::println("unknown argument '{}'", arg);
		print_usage();
		return -1;
	}
}