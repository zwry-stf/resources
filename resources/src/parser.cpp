#include <resources/parser.h>

#include <zstd.h>


namespace resources {
    bool parse(const std::uint8_t* data, std::size_t size,
        std::int32_t id, parsed_file& out) {
        if (!validate(data, size)) {
            return false;
        }
        global_header header;
        std::memcpy(&header, data, sizeof(header));
        for (std::uint32_t i = 0u; i < header.num_files; ++i) {
            file_header file;
            std::memcpy(&file, data + sizeof(header) + static_cast<std::size_t>(i) * sizeof(file), sizeof(file));
            if (file.id != id) {
                continue;
            }
            const auto* start = data + header.data_pointer + file.data_offset;
            std::vector<std::uint8_t> contents(start, start + (file.data_size & k_data_size_mask));
            if (header.use_encryption) {
                decrypt_file(file, contents.data(), contents.size());
            }
            if (file.data_size & k_compressed_flag) {
                std::uint32_t original_size;
                std::memcpy(&original_size, contents.data(), sizeof(original_size));
                if (original_size < k_compression_threshold || original_size > k_max_resource_size) {
                    return false;
                }
                out.data.resize(original_size);
                const auto* compressed = contents.data() + sizeof(original_size);
                const auto compressed_size = contents.size() - sizeof(original_size);
                if (ZSTD_getFrameContentSize(compressed, compressed_size) != original_size ||
                    ZSTD_findFrameCompressedSize(compressed, compressed_size) != compressed_size) {
                    out.data.clear();
                    return false;
                }
                const auto output_size = ZSTD_decompress(out.data.data(), out.data.size(), compressed, compressed_size);
                if (ZSTD_isError(output_size) || output_size != original_size) {
                    out.data.clear();
                    return false;
                }
            }
            else {
                out.data = std::move(contents);
            }
            return true;
        }
        return false;
    }
}