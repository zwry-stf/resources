#pragma once
#include <cstddef>


namespace resources {
	struct file_header {
		int id;
		std::size_t data_offset;
		std::size_t data_size;
	};

	struct global_header {
		std::size_t num_files;
		std::size_t data_pointer;
	};
}