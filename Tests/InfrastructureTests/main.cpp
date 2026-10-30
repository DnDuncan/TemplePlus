
#include "stdafx.h"
#include <windows.h>
#include <fstream>
#include <iterator>
#include <memory>
#include <iostream>
#include <infrastructure/images.h>

int main(int argc, char **argv)
{
	// Uses the engine's decoder on local files, without loading temple.dll or
	// requiring game resources in CI. Output can be compared to source BGRA.
	if (argc == 4 && std::string(argv[1]) == "--decode-image") {
		try {
			std::ifstream input(argv[2], std::ios::binary);
			if (!input) throw std::runtime_error("Cannot open image input");
			std::vector<uint8_t> bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			auto decoded = gfx::DecodeImage(bytes);
			std::ofstream output(argv[3], std::ios::binary | std::ios::trunc);
			output.write(reinterpret_cast<const char*>(decoded.data.get()),
				static_cast<std::streamsize>(decoded.info.width) * decoded.info.height * 4);
			if (!output) throw std::runtime_error("Cannot write decoded image output");
			std::cout << decoded.info.width << "x" << decoded.info.height << " BGRA\n";
			return 0;
		} catch (const std::exception &error) {
			std::cerr << error.what() << "\n";
			return 2;
		}
	}
	TCHAR path[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, path);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
