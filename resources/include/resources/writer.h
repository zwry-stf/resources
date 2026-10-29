#pragma once
#include "parser.h"

#include <limits>
#include <zstd.h>


namespace resources {
    [[nodiscard]] inline bool compress_file(std::vector<std::uint8_t>& data, bool& compressed) {
        compressed = false;
        if (data.size() > k_max_resource_size) {
            return false;
        }
        if (data.size() < k_compression_threshold) {
            return true;
        }
        const auto original_size = static_cast<std::uint32_t>(data.size());
        const auto capacity = ZSTD_compressBound(original_size);
        std::vector<std::uint8_t> output(sizeof(original_size) + capacity);
        const auto compressed_size = ZSTD_compress(output.data() + sizeof(original_size), capacity,
            data.data(), original_size, 22);
        if (ZSTD_isError(compressed_size)) {
            return false;
        }
        if (sizeof(original_size) + compressed_size < data.size()) {
            std::memcpy(output.data(), &original_size, sizeof(original_size));
            output.resize(sizeof(original_size) + compressed_size);
            data = std::move(output);
            compressed = true;
        }
        return true;
    }

    [[nodiscard]] inline bool compress_archive(std::vector<std::uint8_t>& data) {
        if (!validate(data.data(), data.size())) {
            return false;
        }
        global_header header;
        std::memcpy(&header, data.data(), sizeof(header));
        std::vector<std::uint8_t> output(header.data_pointer);
        std::memcpy(output.data(), data.data(), header.data_pointer);
        for (std::uint32_t i = 0u; i < header.num_files; ++i) {
            const auto table_offset = sizeof(header) + static_cast<std::size_t>(i) * sizeof(file_header);
            file_header file;
            std::memcpy(&file, data.data() + table_offset, sizeof(file));
            const auto* start = data.data() + header.data_pointer + file.data_offset;
            std::vector<std::uint8_t> contents(start, start + (file.data_size & k_data_size_mask));
            if (!(file.data_size & k_compressed_flag)) {
                if (header.use_encryption) {
                    decrypt_file(file, contents.data(), contents.size());
                }
                bool compressed;
                if (!compress_file(contents, compressed)) {
                    return false;
                }
                file.data_size = static_cast<std::uint32_t>(contents.size()) | (compressed ? k_compressed_flag : 0u);
                if (header.use_encryption) {
                    decrypt_file(file, contents.data(), contents.size());
                }
            }
            else {
                parsed_file decoded;
                if (!parse(data.data(), data.size(), file.id, decoded)) {
                    return false;
                }
            }
            if (output.size() > std::numeric_limits<std::uint32_t>::max() - contents.size()) {
                return false;
            }
            file.data_offset = static_cast<std::uint32_t>(output.size() - header.data_pointer);
            std::memcpy(output.data() + table_offset, &file, sizeof(file));
            output.insert(output.end(), contents.begin(), contents.end());
        }
        data = std::move(output);
        return true;
    }
}