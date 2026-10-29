#pragma once
#include "common.h"

#include <functional>
#include <vector>


namespace resources {
    struct parsed_file {
        std::vector<std::uint8_t> data;
    };
    struct iterator_t {
        file_header header;
        const std::uint8_t* data;
        std::size_t size;
    };

    [[nodiscard]] inline bool validate(const std::uint8_t* data, std::size_t size) noexcept {
        if (data == nullptr || size < sizeof(global_header)) {
            return false;
        }
        global_header header;
        std::memcpy(&header, data, sizeof(header));
        if (header.num_files > (size - sizeof(header)) / sizeof(file_header) ||
            header.data_pointer < sizeof(header) + static_cast<std::size_t>(header.num_files) * sizeof(file_header) ||
            header.data_pointer > size || header.use_encryption > 1u) {
            return false;
        }
        const auto payload_size = size - header.data_pointer;
        for (std::uint32_t i = 0u; i < header.num_files; ++i) {
            file_header file;
            std::memcpy(&file, data + sizeof(header) + static_cast<std::size_t>(i) * sizeof(file), sizeof(file));
            if (file.data_offset > payload_size || (file.data_size & k_data_size_mask) > payload_size - file.data_offset ||
                (file.data_size & k_data_size_mask) > k_max_resource_size ||
                ((file.data_size & k_compressed_flag) && (file.data_size & k_data_size_mask) < sizeof(std::uint32_t))) {
                return false;
            }
        }
        return true;
    }

    inline void for_each_resource(const std::uint8_t* data, std::size_t size,
        std::function<void(const iterator_t&)>&& callback) {
        if (!validate(data, size)) {
            return;
        }
        global_header header;
        std::memcpy(&header, data, sizeof(header));
        for (std::uint32_t i = 0u; i < header.num_files; ++i) {
            file_header file;
            std::memcpy(&file, data + sizeof(header) + static_cast<std::size_t>(i) * sizeof(file), sizeof(file));
            callback({file, data + header.data_pointer + file.data_offset, (file.data_size & k_data_size_mask)});
        }
    }

    inline void decrypt_file(const file_header& header, std::uint8_t* data, std::size_t size) {
        std::uint8_t key[sizeof(header.encryption_key)];
        derive_rc4_key(header.encryption_key, key);
        rc4_crypt(data, size, key, sizeof(key));
    }

    [[nodiscard]] bool parse(const std::uint8_t* data, std::size_t size,
        std::int32_t id, parsed_file& out);
}