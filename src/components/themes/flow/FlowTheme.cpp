#include "FlowTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdint>
#include <string>

#include "ReadingStatsStore.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/cover.h"
#include "fontIds.h"

namespace {
constexpr int cornerRadius = 6;
constexpr int sideCoverWidth = 150;
constexpr int sideCoverHeight = 200;
constexpr int centerCoverWidth = 220;
constexpr int centerCoverHeight = 294;
constexpr int hPadding = 10;
}  // namespace

void FlowTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                   const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                   bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const bool hasRecentBooks = !recentBooks.empty();
  const int pageWidth = renderer.getScreenWidth();
  const int centerY = rect.y + 10;
  const int centerX = pageWidth / 2;
  if (hasRecentBooks) {
    int count = recentBooks.size();
    // Use bookSelectorIndex logic if possible. 
    // If selectorIndex >= 1000, it means focus is elsewhere but we center on (selectorIndex - 1000)
    bool hasSelection = (selectorIndex >= 0 && selectorIndex < count);
    int curIdx = hasSelection ? selectorIndex : (selectorIndex >= 1000 ? (selectorIndex - 1000) : 0);
    if (curIdx >= count) curIdx = 0;
    
    // Per user request: [ 2 1 3 ] order
    // We want to show up to 2 side covers
    auto drawStackedCover = [&](int idx, bool isLeft) {
        int w = sideCoverWidth;
        int h = sideCoverHeight; // 160
        
        // Offset for side covers to peek from behind
        int xOffset = (centerCoverWidth / 2) + 10; // Reduced gap to overlap more
        int drawX = isLeft ? (centerX - xOffset - w + 80) : (centerX + xOffset - 80);
        int drawY = centerY + (centerCoverHeight - h) / 2;
        
        const std::string coverPath = UITheme::getCoverThumbPath(recentBooks[idx].coverBmpPath, h);
        FsFile file;
        
        bool success = false;
        if (!coverPath.empty() && Storage.openFileForRead("HOME", coverPath, file)) {
            Bitmap bitmap(file);
            if (bitmap.parseHeaders() == BmpReaderError::Ok) {
                renderer.drawBitmap(bitmap, drawX, drawY, w, h);
                success = true;
            }
            file.close();
        }
        
        if (!success) {
            renderer.fillRect(drawX, drawY, w, h, false); // Clear
            renderer.drawRect(drawX, drawY, w, h, true);
        } else {
            renderer.drawRect(drawX, drawY, w, h, true);
        }
    };

    // Spatial mapping per user request: [ 2 1 3 4 5 6 ]
    int leftIdx = -1, rightIdx = -1;
    if (count > 1) {
        // next logic
        if (curIdx == 1) rightIdx = 0;
        else if (curIdx == 0) rightIdx = (count > 2) ? 2 : 1;
        else if (curIdx == count - 1) rightIdx = 1;
        else rightIdx = curIdx + 1;

        // prev logic
        if (curIdx == 0) leftIdx = 1;
        else if (curIdx == 1) leftIdx = count - 1;
        else if (curIdx == 2) leftIdx = 0;
        else leftIdx = curIdx - 1;
    }

    // Draw stacked covers from outside in (behind to front)
    if (rightIdx != -1) {
        drawStackedCover(rightIdx, false);
    }
    if (leftIdx != -1) {
        drawStackedCover(leftIdx, true);
    }

    // Draw Center Cover (Current)
    {
        const std::string coverPath = UITheme::getCoverThumbPath(recentBooks[curIdx].coverBmpPath, centerCoverHeight);
        FsFile file;
        int drawX = centerX - centerCoverWidth / 2;
        int drawY = centerY;
        
        // Clear background for center cover to ensure it "covers" sides
        renderer.fillRect(drawX, drawY, centerCoverWidth, centerCoverHeight, false);

        bool success = false;
        if (!coverPath.empty() && Storage.openFileForRead("HOME", coverPath, file)) {
            Bitmap bitmap(file);
            if (bitmap.parseHeaders() == BmpReaderError::Ok) {
                renderer.drawBitmap(bitmap, drawX, drawY, centerCoverWidth, centerCoverHeight);
                success = true;
            }
            file.close();
        }
        
        renderer.drawRect(drawX, drawY, centerCoverWidth, centerCoverHeight, true);
        if (!success) {
             renderer.fillRect(drawX, drawY + centerCoverHeight/3, centerCoverWidth, 2*centerCoverHeight/3, true);
             renderer.drawIcon(CoverIcon, drawX + centerCoverWidth/2 - 16, drawY + centerCoverHeight/2 - 16, 32, 32);
        }

        if (hasSelection) {
            // Highlight border if selected (Book focus)
            renderer.drawRect(drawX - 2, drawY - 2, centerCoverWidth + 4, centerCoverHeight + 4, 3, true);
        }

        // Draw File Name below center cover
        std::string filename = recentBooks[curIdx].path;
        size_t lastSlash = filename.find_last_of('/');
        if (lastSlash != std::string::npos) filename = filename.substr(lastSlash + 1);
        size_t lastDot = filename.find_last_of('.');
        if (lastDot != std::string::npos && lastDot > 0) filename = filename.substr(0, lastDot);
        
        auto truncatedTitle = renderer.truncatedText(UI_12_FONT_ID, filename.c_str(), pageWidth - 40);
        int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, truncatedTitle.c_str());
        renderer.drawText(UI_12_FONT_ID, centerX - titleWidth / 2, drawY + centerCoverHeight + 10, truncatedTitle.c_str(), true);
    }
    
    coverRendered = true;
    coverBufferStored = false; 

  } else {
    drawEmptyRecents(renderer, rect);
  }
  
  // Also draw the footer (Total Reading Time)
  drawFooter(renderer);
}

void FlowTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                              const std::function<std::string(int index)>& buttonLabel,
                              const std::function<UIIcon(int index)>& rowIcon) const {
  const int rowHeight = FlowMetrics::values.menuRowHeight;
  const int spacing = FlowMetrics::values.menuSpacing;
  
  // Align menu with the leftmost cover (B2)
  // Left edge of B2 is at centerX - 190
  const int centerX = rect.width / 2;
  const int menuLeft = centerX - 190;
  const int menuWidth = 380; // Symmetric width
  
  for (int i = 0; i < buttonCount; ++i) {
    const bool selected = (selectedIndex == i);
    int y = rect.y + i * (rowHeight + spacing);
    
    if (selected) {
      renderer.fillRoundedRect(menuLeft, y, menuWidth, rowHeight, cornerRadius, Color::LightGray);
    }
    
    // Use LyraTheme's icon drawing logic
    if (rowIcon != nullptr) {
      UIIcon icon = rowIcon(i);
      const uint8_t* iconBitmap = LyraTheme::iconForName(icon, 32);
      if (iconBitmap != nullptr) {
        renderer.drawIcon(iconBitmap, menuLeft + 12, y + (rowHeight - 32) / 2, 32, 32);
      }
    }
    
    std::string label = buttonLabel(i);
    renderer.drawText(UI_12_FONT_ID, menuLeft + rowHeight, y + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, label.c_str(), true);
  }
}

void FlowTheme::drawFooter(GfxRenderer& renderer) const {
    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();
    
    uint32_t totalSeconds = READING_STATS.totalReadingSeconds;
    uint32_t hours = totalSeconds / 3600;
    uint32_t minutes = (totalSeconds % 3600) / 60;
    
    char timeStr[48];
    snprintf(timeStr, sizeof(timeStr), "You have read for %uh %um", hours, minutes);
    
    int textWidth = renderer.getTextWidth(SMALL_FONT_ID, timeStr);
    renderer.drawText(SMALL_FONT_ID, pageWidth - textWidth - 25, pageHeight - 30, timeStr, true);
}
