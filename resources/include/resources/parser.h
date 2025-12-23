#pragma once
#include "common.h"
#include <cstdint>
#include <cstring>


namespace resources {
	struct parsed_file {
		const std::uint8_t* data{};
		std::size_t size{};
	};

	bool parse(const std::uint8_t* arr, std::size_t size, int id, parsed_file& out) {
		if (size < sizeof(global_header))
			return false;

		global_header header;
		std::memcpy(
			&header, 
			arr, 
			sizeof(header)
		);

		if (header.num_files == 0u)
			return false;

		for (std::size_t i = 0u; i < header.num_files; i++) {
			const std::size_t offset = sizeof(header) + i * sizeof(file_header);
			if (size < offset + sizeof(file_header))
				return false;

			file_header fheader;
			std::memcpy(
				&fheader,
				arr + offset,
				sizeof(fheader)
			);

			if (fheader.id == id) {
				out.data = arr + header.data_pointer + fheader.data_offset;
				out.size = fheader.data_size;
				return true;
			}
		}

		return false;
	}
}