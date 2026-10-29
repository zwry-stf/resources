#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>


namespace resources {
    inline constexpr std::uint32_t k_compressed_flag = 0x80000000u;
    inline constexpr std::uint32_t k_data_size_mask = ~k_compressed_flag;
    inline constexpr std::size_t k_compression_threshold = 1024u;
    inline constexpr std::uint32_t k_max_resource_size = 256u * 1024u * 1024u;
	struct file_header {
		std::int32_t id;
		std::uint32_t data_offset;
		std::uint32_t data_size;
        std::uint8_t encryption_key[32];
	};

#pragma pack(push, 1)
	struct global_header {
		std::uint32_t num_files;
		std::uint32_t data_pointer;
        std::uint8_t use_encryption{ 0 };
	};
#pragma pack(pop)

    inline void rc4_crypt(std::uint8_t* data, std::size_t len,
                          const std::uint8_t* key, std::size_t key_len) {
        if (key_len == 0 || len == 0) {
            return;
        }

        std::uint8_t S[256];
        for (int i = 0; i < 256; ++i) {
            S[i] = static_cast<std::uint8_t>(i);
        }

        int j = 0;
        for (int i = 0; i < 256; ++i) {
            j = (j + S[i] + key[i % key_len]) & 0xFF;
            std::swap(S[i], S[j]);
        }

        int i = 0;
        j = 0;
        for (std::size_t n = 0; n < len; ++n) {
            i = (i + 1) & 0xFF;
            j = (j + S[i]) & 0xFF;
            std::swap(S[i], S[j]);
            std::uint8_t k = S[(S[i] + S[j]) & 0xFF];
            data[n] ^= k;
        }
    }
    inline void derive_rc4_key(const std::uint8_t* seed, std::uint8_t* out_key) {
        std::memcpy(out_key, seed, 32);
        for (int i = 0; i < 32; ++i) {
            out_key[i] ^= static_cast<std::uint8_t>(i * 0xA5u + 0x37u);
            out_key[i] = (out_key[i] << 5) | (out_key[i] >> 3);
            out_key[i] ^= seed[(i + 17) % 32];
        }
    }
}