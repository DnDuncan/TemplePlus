#include "stdafx.h"
#include <memory>
#include <vector>
#include <infrastructure/images.h>
#include <infrastructure/exception.h>

// Synthetic 8-bit PNGs generated from tiny known pixel arrays, no game art.
static std::vector<uint8_t> RgbaPng() {
	return {
		137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
		0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13,
		36, 0, 0, 0, 26, 73, 68, 65, 84, 120, 218, 99, 224, 18, 145, 251,
		175, 97, 100, 195, 192, 224, 22, 16, 213, 144, 146, 87, 241, 31, 0, 38,
		105, 5, 139, 63, 104, 49, 100, 0, 0, 0, 0, 73, 69, 78, 68, 174,
		66, 96, 130
	};
}

static std::vector<uint8_t> RgbPng() {
	return {
		137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
		0, 0, 0, 2, 0, 0, 0, 1, 8, 2, 0, 0, 0, 123, 64, 232,
		221, 0, 0, 0, 15, 73, 68, 65, 84, 120, 218, 99, 224, 22, 83, 212,
		49, 119, 2, 0, 2, 111, 0, 232, 77, 96, 121, 120, 0, 0, 0, 0,
		73, 69, 78, 68, 174, 66, 96, 130
	};
}

TEST(ImageTests, PngPreservesDimensionsAlphaChannelOrderAndTopDownRows) {
    auto bytes = RgbaPng();
    auto info = gfx::DetectImageFormat(bytes);
    EXPECT_EQ(gfx::ImageFileFormat::PNG, info.format);
    EXPECT_EQ(2, info.width);
    EXPECT_EQ(2, info.height);
    EXPECT_TRUE(info.hasAlpha);
    auto decoded = gfx::DecodeImage(bytes);
    ASSERT_TRUE(decoded.data != nullptr);
    const uint8_t expected[] = { 30,20,10,255, 60,50,40,0, 90,80,70,128, 120,110,100,255 };
    for (size_t i = 0; i < sizeof(expected); ++i) EXPECT_EQ(expected[i], decoded.data[i]);
}

TEST(ImageTests, RgbPngGetsOpaqueAlphaWithoutColorConversion) {
    auto bytes = RgbPng();
    auto decoded = gfx::DecodeImage(bytes);
    EXPECT_EQ(gfx::ImageFileFormat::PNG, decoded.info.format);
    EXPECT_EQ(2, decoded.info.width);
    EXPECT_EQ(1, decoded.info.height);
    EXPECT_FALSE(decoded.info.hasAlpha);
    ASSERT_TRUE(decoded.data != nullptr);
    const uint8_t expected[] = { 33,22,11,255, 66,55,44,255 };
    for (size_t i = 0; i < sizeof(expected); ++i) EXPECT_EQ(expected[i], decoded.data[i]);
}

TEST(ImageTests, EmptyAndTruncatedPngAreRejected) {
    auto source = RgbaPng();
    for (size_t size = 0; size <= 33; ++size) {
        std::vector<uint8_t> truncated(source.begin(), source.begin() + size);
        EXPECT_THROW(gfx::DecodeImage(truncated), TempleException);
    }
    source[41] = 0; // Invalid zlib compression header in IDAT, valid IHDR.
    EXPECT_THROW(gfx::DecodeImage(source), TempleException);
}

TEST(ImageTests, OversizedPngIsRejectedBeforePixelAllocation) {
    auto bytes = RgbaPng();
    // 65535x65535 dimensions in IHDR. Header metadata is inspected before allocation.
    bytes[16] = bytes[17] = bytes[20] = bytes[21] = 0;
    bytes[18] = bytes[19] = bytes[22] = bytes[23] = 255;
    EXPECT_THROW(gfx::DecodeImage(bytes), TempleException);
}

TEST(ImageTests, ExistingTgaPathStillDecodesBgraAndVerticalOrder) {
    std::vector<uint8_t> bytes(18, 0);
    bytes[2] = 2; bytes[12] = 1; bytes[14] = 2; bytes[16] = 32;
    // TGA bottom row, followed by top row.
    const uint8_t pixels[] = { 90,80,70,128, 30,20,10,255 };
    bytes.insert(bytes.end(), pixels, pixels + sizeof(pixels));
    auto decoded = gfx::DecodeImage(bytes);
    EXPECT_EQ(gfx::ImageFileFormat::TGA, decoded.info.format);
    EXPECT_TRUE(decoded.info.hasAlpha);
    const uint8_t expected[] = { 30,20,10,255, 90,80,70,128 };
    for (size_t i = 0; i < sizeof(expected); ++i) EXPECT_EQ(expected[i], decoded.data[i]);
}
