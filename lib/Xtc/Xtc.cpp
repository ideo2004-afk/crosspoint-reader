/**
 * Xtc.cpp
 *
 * Main XTC ebook class implementation
 * XTC ebook support for CrossPoint Reader
 */

#include "Xtc.h"

#include <HalStorage.h>
#include <Logging.h>

bool Xtc::load() {
  LOG_DBG("XTC", "Loading XTC: %s", filepath.c_str());

  // Initialize parser
  parser.reset(new xtc::XtcParser());

  // Open XTC file
  xtc::XtcError err = parser->open(filepath.c_str());
  if (err != xtc::XtcError::OK) {
    LOG_ERR("XTC", "Failed to load: %s", xtc::errorToString(err));
    parser.reset();
    return false;
  }

  loaded = true;
  LOG_DBG("XTC", "Loaded XTC: %s (%lu pages)", filepath.c_str(), parser->getPageCount());
  return true;
}

bool Xtc::clearCache() const {
  if (!Storage.exists(cachePath.c_str())) {
    LOG_DBG("XTC", "Cache does not exist, no action needed");
    return true;
  }

  if (!Storage.removeDir(cachePath.c_str())) {
    LOG_ERR("XTC", "Failed to clear cache");
    return false;
  }

  LOG_DBG("XTC", "Cache cleared successfully");
  return true;
}

void Xtc::setupCacheDir() const {
  if (Storage.exists(cachePath.c_str())) {
    return;
  }

  // Create directories recursively
  for (size_t i = 1; i < cachePath.length(); i++) {
    if (cachePath[i] == '/') {
      Storage.mkdir(cachePath.substr(0, i).c_str());
    }
  }
  Storage.mkdir(cachePath.c_str());
}

std::string Xtc::getTitle() const {
  if (!loaded || !parser) {
    return "";
  }

  // Try to get title from XTC metadata first
  std::string title = parser->getTitle();
  if (!title.empty()) {
    return title;
  }

  // Fallback: extract filename from path as title
  size_t lastSlash = filepath.find_last_of('/');
  size_t lastDot = filepath.find_last_of('.');

  if (lastSlash == std::string::npos) {
    lastSlash = 0;
  } else {
    lastSlash++;
  }

  if (lastDot == std::string::npos || lastDot <= lastSlash) {
    return filepath.substr(lastSlash);
  }

  return filepath.substr(lastSlash, lastDot - lastSlash);
}

std::string Xtc::getAuthor() const {
  if (!loaded || !parser) {
    return "";
  }

  // Try to get author from XTC metadata
  return parser->getAuthor();
}

bool Xtc::hasChapters() const {
  if (!loaded || !parser) {
    return false;
  }
  return parser->hasChapters();
}

const std::vector<xtc::ChapterInfo>& Xtc::getChapters() const {
  static const std::vector<xtc::ChapterInfo> kEmpty;
  if (!loaded || !parser) {
    return kEmpty;
  }
  return parser->getChapters();
}

std::string Xtc::getCoverBmpPath() const { return cachePath + "/cover.bmp"; }

bool Xtc::generateCoverBmp() const {
  // Already generated
  if (Storage.exists(getCoverBmpPath().c_str())) {
    return true;
  }

  if (!loaded || !parser) {
    LOG_ERR("XTC", "Cannot generate cover BMP, file not loaded");
    return false;
  }

  if (parser->getPageCount() == 0) {
    LOG_ERR("XTC", "No pages in XTC file");
    return false;
  }

  // Setup cache directory
  setupCacheDir();

  // Get first page info for cover
  xtc::PageInfo pageInfo;
  if (!parser->getPageInfo(0, pageInfo)) {
    LOG_DBG("XTC", "Failed to get first page info");
    return false;
  }

  // Get bit depth
  const uint8_t bitDepth = parser->getBitDepth();

  // Allocate buffer for page data
  // XTG (1-bit): Row-major, ((width+7)/8) * height bytes
  // XTH (2-bit): Two bit planes, column-major, ((width * height + 7) / 8) * 2 bytes
  size_t bitmapSize;
  if (bitDepth == 2) {
    bitmapSize = ((static_cast<size_t>(pageInfo.width) * pageInfo.height + 7) / 8) * 2;
  } else {
    bitmapSize = ((pageInfo.width + 7) / 8) * pageInfo.height;
  }
  uint8_t* pageBuffer = static_cast<uint8_t*>(malloc(bitmapSize));
  if (!pageBuffer) {
    LOG_ERR("XTC", "Failed to allocate page buffer (%lu bytes)", bitmapSize);
    return false;
  }

  // Load first page (cover)
  size_t bytesRead = const_cast<xtc::XtcParser*>(parser.get())->loadPage(0, pageBuffer, bitmapSize);
  if (bytesRead == 0) {
    LOG_ERR("XTC", "Failed to load cover page");
    free(pageBuffer);
    return false;
  }

  // Create BMP file
  FsFile coverBmp;
  if (!Storage.openFileForWrite("XTC", getCoverBmpPath(), coverBmp)) {
    LOG_DBG("XTC", "Failed to create cover BMP file");
    free(pageBuffer);
    return false;
  }

  // Write BMP header
  // BMP file header (14 bytes)
  const uint32_t rowSize = ((pageInfo.width + 31) / 32) * 4;  // Row size aligned to 4 bytes
  const uint32_t imageSize = rowSize * pageInfo.height;
  const uint32_t fileSize = 14 + 40 + 8 + imageSize;  // Header + DIB + palette + data

  // File header
  coverBmp.write('B');
  coverBmp.write('M');
  coverBmp.write(reinterpret_cast<const uint8_t*>(&fileSize), 4);
  uint32_t reserved = 0;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&reserved), 4);
  uint32_t dataOffset = 14 + 40 + 8;  // 1-bit palette has 2 colors (8 bytes)
  coverBmp.write(reinterpret_cast<const uint8_t*>(&dataOffset), 4);

  // DIB header (BITMAPINFOHEADER - 40 bytes)
  uint32_t dibHeaderSize = 40;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&dibHeaderSize), 4);
  int32_t width = pageInfo.width;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&width), 4);
  int32_t height = -static_cast<int32_t>(pageInfo.height);  // Negative for top-down
  coverBmp.write(reinterpret_cast<const uint8_t*>(&height), 4);
  uint16_t planes = 1;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&planes), 2);
  uint16_t bitsPerPixel = 1;  // 1-bit monochrome
  coverBmp.write(reinterpret_cast<const uint8_t*>(&bitsPerPixel), 2);
  uint32_t compression = 0;  // BI_RGB (no compression)
  coverBmp.write(reinterpret_cast<const uint8_t*>(&compression), 4);
  coverBmp.write(reinterpret_cast<const uint8_t*>(&imageSize), 4);
  int32_t ppmX = 2835;  // 72 DPI
  coverBmp.write(reinterpret_cast<const uint8_t*>(&ppmX), 4);
  int32_t ppmY = 2835;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&ppmY), 4);
  uint32_t colorsUsed = 2;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&colorsUsed), 4);
  uint32_t colorsImportant = 2;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&colorsImportant), 4);

  // Color palette (2 colors for 1-bit)
  // XTC 1-bit polarity: 0 = black, 1 = white (standard BMP palette order)
  // Color 0: Black (text/foreground in XTC)
  uint8_t black[4] = {0x00, 0x00, 0x00, 0x00};
  coverBmp.write(black, 4);
  // Color 1: White (background in XTC)
  uint8_t white[4] = {0xFF, 0xFF, 0xFF, 0x00};
  coverBmp.write(white, 4);

  // Write bitmap data
  // BMP requires 4-byte row alignment
  const size_t dstRowSize = (pageInfo.width + 7) / 8;  // 1-bit destination row size

  if (bitDepth == 2) {
    // XTH 2-bit mode: Two bit planes, column-major order
    // - Columns scanned right to left (x = width-1 down to 0)
    // - 8 vertical pixels per byte (MSB = topmost pixel in group)
    // - First plane: Bit1, Second plane: Bit2
    // - Pixel value = (bit1 << 1) | bit2
    const size_t planeSize = (static_cast<size_t>(pageInfo.width) * pageInfo.height + 7) / 8;
    const uint8_t* plane1 = pageBuffer;                 // Bit1 plane
    const uint8_t* plane2 = pageBuffer + planeSize;     // Bit2 plane
    const size_t colBytes = (pageInfo.height + 7) / 8;  // Bytes per column

    // Allocate a row buffer for 1-bit output
    uint8_t* rowBuffer = static_cast<uint8_t*>(malloc(dstRowSize));
    if (!rowBuffer) {
      free(pageBuffer);
      coverBmp.close();
      return false;
    }

    for (uint16_t y = 0; y < pageInfo.height; y++) {
      memset(rowBuffer, 0xFF, dstRowSize);  // Start with all white

      for (uint16_t x = 0; x < pageInfo.width; x++) {
        // Column-major, right to left: column index = (width - 1 - x)
        const size_t colIndex = pageInfo.width - 1 - x;
        const size_t byteInCol = y / 8;
        const size_t bitInByte = 7 - (y % 8);  // MSB = topmost pixel

        const size_t byteOffset = colIndex * colBytes + byteInCol;
        const uint8_t bit1 = (plane1[byteOffset] >> bitInByte) & 1;
        const uint8_t bit2 = (plane2[byteOffset] >> bitInByte) & 1;
        const uint8_t pixelValue = (bit1 << 1) | bit2;

        // Threshold: 0=white (1); 1,2,3=black (0)
        if (pixelValue >= 1) {
          // Set bit to 0 (black) in BMP format
          const size_t dstByte = x / 8;
          const size_t dstBit = 7 - (x % 8);
          rowBuffer[dstByte] &= ~(1 << dstBit);
        }
      }

      // Write converted row
      coverBmp.write(rowBuffer, dstRowSize);

      // Pad to 4-byte boundary
      uint8_t padding[4] = {0, 0, 0, 0};
      size_t paddingSize = rowSize - dstRowSize;
      if (paddingSize > 0) {
        coverBmp.write(padding, paddingSize);
      }
    }

    free(rowBuffer);
  } else {
    // 1-bit source: write directly with proper padding
    const size_t srcRowSize = (pageInfo.width + 7) / 8;

    for (uint16_t y = 0; y < pageInfo.height; y++) {
      // Write source row
      coverBmp.write(pageBuffer + y * srcRowSize, srcRowSize);

      // Pad to 4-byte boundary
      uint8_t padding[4] = {0, 0, 0, 0};
      size_t paddingSize = rowSize - srcRowSize;
      if (paddingSize > 0) {
        coverBmp.write(padding, paddingSize);
      }
    }
  }

  coverBmp.close();
  free(pageBuffer);

  LOG_DBG("XTC", "Generated cover BMP: %s", getCoverBmpPath().c_str());
  return true;
}

std::string Xtc::getThumbBmpPath() const { return cachePath + "/thumb_[HEIGHT].bmp"; }
std::string Xtc::getThumbBmpPath(int height) const { return cachePath + "/thumb_" + std::to_string(height) + ".bmp"; }

bool Xtc::generateThumbBmp(int height) const {
  // Already generated
  if (Storage.exists(getThumbBmpPath(height).c_str())) {
    return true;
  }

  if (!loaded || !parser) {
    LOG_ERR("XTC", "Cannot generate thumb BMP, file not loaded");
    return false;
  }

  if (parser->getPageCount() == 0) {
    LOG_ERR("XTC", "No pages in XTC file");
    return false;
  }

  // Setup cache directory
  setupCacheDir();

  // Get first page info for cover
  xtc::PageInfo pageInfo;
  if (!parser->getPageInfo(0, pageInfo)) {
    LOG_DBG("XTC", "Failed to get first page info");
    return false;
  }

  // Get bit depth
  const uint8_t bitDepth = parser->getBitDepth();

  // Calculate target dimensions for thumbnail (fit within 3x3 grid or Home card)
  // Use 0.75 (3:4) aspect ratio to fill selection boxes better
  int THUMB_TARGET_WIDTH = height * 0.75;
  int THUMB_TARGET_HEIGHT = height;

  // Detect content bounding box to remove white margins
  uint16_t contentXStart = pageInfo.width;
  uint16_t contentXEnd = 0;
  uint16_t contentYStart = pageInfo.height;
  uint16_t contentYEnd = 0;

  const size_t bpcSize = (bitDepth == 2) ? ((static_cast<size_t>(pageInfo.width) * pageInfo.height + 7) / 8) : 0;
  const uint8_t* bpcPlane1 = (bitDepth == 2) ? pageBuffer : nullptr;
  const uint8_t* bpcPlane2 = (bitDepth == 2) ? pageBuffer + bpcSize : nullptr;
  const size_t bpcColBytes = (bitDepth == 2) ? ((pageInfo.height + 7) / 8) : 0;
  const size_t bpcSrcRowBytes = (bitDepth == 1) ? ((pageInfo.width + 7) / 8) : 0;

  for (uint16_t y = 0; y < pageInfo.height; y++) {
    for (uint16_t x = 0; x < pageInfo.width; x++) {
      bool isBlack = false;
      if (bitDepth == 2) {
        const size_t colIndex = pageInfo.width - 1 - x;
        const size_t byteOffset = colIndex * bpcColBytes + (y / 8);
        const uint8_t bit1 = (bpcPlane1[byteOffset] >> (7 - (y % 8))) & 1;
        const uint8_t bit2 = (bpcPlane2[byteOffset] >> (7 - (y % 8))) & 1;
        if ((bit1 << 1 | bit2) > 0) isBlack = true; // Any non-white
      } else {
        if (!((pageBuffer[y * bpcSrcRowBytes + x / 8] >> (7 - (x % 8))) & 1)) isBlack = true;
      }

      if (isBlack) {
        if (x < contentXStart) contentXStart = x;
        if (x > contentXEnd) contentXEnd = x;
        if (y < contentYStart) contentYStart = y;
        if (y > contentYEnd) contentYEnd = y;
      }
    }
  }

  // Fallback if page is entirely white
  if (contentXStart >= contentXEnd || contentYStart >= contentYEnd) {
    contentXStart = 0;
    contentXEnd = pageInfo.width - 1;
    contentYStart = 0;
    contentYEnd = pageInfo.height - 1;
  }
  
  // Add 2px safety margin to the crop
  contentXStart = (contentXStart > 2) ? contentXStart - 2 : 0;
  contentYStart = (contentYStart > 2) ? contentYStart - 2 : 0;
  contentXEnd = (contentXEnd + 2 < pageInfo.width) ? contentXEnd + 2 : pageInfo.width - 1;
  contentYEnd = (contentYEnd + 2 < pageInfo.height) ? contentYEnd + 2 : pageInfo.height - 1;

  uint16_t contentWidth = contentXEnd - contentXStart + 1;
  uint16_t contentHeight = contentYEnd - contentYStart + 1;

  // Calculate scale factor from content box
  float scaleX = static_cast<float>(THUMB_TARGET_WIDTH) / contentWidth;
  float scaleY = static_cast<float>(THUMB_TARGET_HEIGHT) / contentHeight;
  float scale = (scaleX < scaleY) ? scaleX : scaleY; // Fit content within target

  uint16_t thumbWidth = static_cast<uint16_t>(contentWidth * scale);
  uint16_t thumbHeight = static_cast<uint16_t>(contentHeight * scale);

  LOG_DBG("XTC", "Generating thumb BMP (Autocrop): %dx%d @ (%d,%d) -> %dx%d (scale: %.3f)", 
          contentWidth, contentHeight, contentXStart, contentYStart, thumbWidth, thumbHeight, scale);

  // Create thumbnail BMP file
  FsFile thumbBmp;
  if (!Storage.openFileForWrite("XTC", getThumbBmpPath(height), thumbBmp)) {
    LOG_DBG("XTC", "Failed to create thumb BMP file");
    free(pageBuffer);
    return false;
  }

  // Write 1-bit BMP header
  const uint32_t rowSize = (thumbWidth + 31) / 32 * 4;
  const uint32_t imageSize = rowSize * thumbHeight;
  const uint32_t fileSize = 14 + 40 + 8 + imageSize;

  // File header
  thumbBmp.write('B');
  thumbBmp.write('M');
  write32(thumbBmp, fileSize);
  write32(thumbBmp, 0);
  write32(thumbBmp, 62);

  // DIB header
  write32(thumbBmp, 40);
  write32Signed(thumbBmp, thumbWidth);
  write32Signed(thumbBmp, -thumbHeight);
  write16(thumbBmp, 1);
  write16(thumbBmp, 1);
  write32(thumbBmp, 0);
  write32(thumbBmp, imageSize);
  write32(thumbBmp, 2835);
  write32(thumbBmp, 2835);
  write32(thumbBmp, 2);
  write32(thumbBmp, 2);

  // Color palette
  uint8_t palette[8] = {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00};
  thumbBmp.write(palette, 8);

  uint8_t* rowBuffer = static_cast<uint8_t*>(malloc(rowSize));
  if (!rowBuffer) {
    free(pageBuffer);
    thumbBmp.close();
    return false;
  }

  // Fixed-point scale factor (16.16)
  uint32_t scaleInv_fp = static_cast<uint32_t>(65536.0f / scale);

  for (uint16_t dstY = 0; dstY < thumbHeight; dstY++) {
    memset(rowBuffer, 0xFF, rowSize);

    uint32_t srcYStart = contentYStart + ((static_cast<uint32_t>(dstY) * scaleInv_fp) >> 16);
    uint32_t srcYEnd = contentYStart + ((static_cast<uint32_t>(dstY + 1) * scaleInv_fp) >> 16);
    
    for (uint16_t dstX = 0; dstX < thumbWidth; dstX++) {
      uint32_t srcXStart = contentXStart + ((static_cast<uint32_t>(dstX) * scaleInv_fp) >> 16);
      uint32_t srcXEnd = contentXStart + ((static_cast<uint32_t>(dstX + 1) * scaleInv_fp) >> 16);

      uint32_t graySum = 0;
      uint32_t totalCount = 0;

      for (uint32_t srcY = srcYStart; srcY < srcYEnd && srcY < pageInfo.height; srcY++) {
        for (uint32_t srcX = srcXStart; srcX < srcXEnd && srcX < pageInfo.width; srcX++) {
          uint8_t grayValue = 255;

          if (bitDepth == 2) {
            const size_t colIndex = pageInfo.width - 1 - srcX;
            const size_t byteOffset = colIndex * bpcColBytes + (srcY / 8);
            const uint8_t bit1 = (bpcPlane1[byteOffset] >> (7 - (srcY % 8))) & 1;
            const uint8_t bit2 = (bpcPlane2[byteOffset] >> (7 - (srcY % 8))) & 1;
            grayValue = (3 - (bit1 << 1 | bit2)) * 85;
          } else {
            grayValue = ((pageBuffer[srcY * bpcSrcRowBytes + srcX / 8] >> (7 - (srcX % 8))) & 1) ? 255 : 0;
          }

          graySum += grayValue;
          totalCount++;
        }
      }

      uint8_t avgGray = (totalCount > 0) ? static_cast<uint8_t>(graySum / totalCount) : 255;
      
      // Use Bayer 8x8 for consistent halftone quality
      static const uint8_t bayer[8][8] = {
          {0, 32, 8, 40, 2, 34, 10, 42}, {48, 16, 56, 24, 50, 18, 58, 26},
          {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
          {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
          {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};
      
      uint8_t bayerVal = bayer[dstY % 8][dstX % 8] * 4;
      uint8_t oneBit = (avgGray >= bayerVal) ? 1 : 0;

      const size_t byteIndex = dstX / 8;
      const size_t bitOffset = 7 - (dstX % 8);
      if (oneBit) rowBuffer[byteIndex] |= (1 << bitOffset);
      else rowBuffer[byteIndex] &= ~(1 << bitOffset);
    }
    thumbBmp.write(rowBuffer, rowSize);
  }

  free(rowBuffer);
  thumbBmp.close();
  free(pageBuffer);

  LOG_DBG("XTC", "Generated thumb BMP (%dx%d): %s", thumbWidth, thumbHeight, getThumbBmpPath(height).c_str());
  return true;
}

uint32_t Xtc::getPageCount() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getPageCount();
}

uint16_t Xtc::getPageWidth() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getWidth();
}

uint16_t Xtc::getPageHeight() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getHeight();
}

uint8_t Xtc::getBitDepth() const {
  if (!loaded || !parser) {
    return 1;  // Default to 1-bit
  }
  return parser->getBitDepth();
}

size_t Xtc::loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize) const {
  if (!loaded || !parser) {
    return 0;
  }
  return const_cast<xtc::XtcParser*>(parser.get())->loadPage(pageIndex, buffer, bufferSize);
}

xtc::XtcError Xtc::loadPageStreaming(uint32_t pageIndex,
                                     std::function<void(const uint8_t* data, size_t size, size_t offset)> callback,
                                     size_t chunkSize) const {
  if (!loaded || !parser) {
    return xtc::XtcError::FILE_NOT_FOUND;
  }
  return const_cast<xtc::XtcParser*>(parser.get())->loadPageStreaming(pageIndex, callback, chunkSize);
}

uint8_t Xtc::calculateProgress(uint32_t currentPage) const {
  if (!loaded || !parser || parser->getPageCount() == 0) {
    return 0;
  }
  return static_cast<uint8_t>((currentPage + 1) * 100 / parser->getPageCount());
}

xtc::XtcError Xtc::getLastError() const {
  if (!parser) {
    return xtc::XtcError::FILE_NOT_FOUND;
  }
  return parser->getLastError();
}
