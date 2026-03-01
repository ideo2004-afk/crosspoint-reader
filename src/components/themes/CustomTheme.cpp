#include "CustomTheme.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Utf8.h>

#include <string>

#include "I18n.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/book.h"
#include "components/icons/book24.h"
#include "components/icons/cover.h"
#include "components/icons/file24.h"
#include "components/icons/folder.h"
#include "components/icons/folder24.h"
#include "components/icons/hotspot.h"
#include "components/icons/image24.h"
#include "components/icons/library.h"
#include "components/icons/recent.h"
#include "components/icons/settings2.h"
#include "components/icons/text24.h"
#include "components/icons/transfer.h"
#include "components/icons/wifi.h"
#include "fontIds.h"

namespace {
const uint8_t* iconForName(UIIcon icon, int size) {
  if (size == 24) {
    switch (icon) {
      case UIIcon::Folder: return Folder24Icon;
      case UIIcon::Text: return Text24Icon;
      case UIIcon::Image: return Image24Icon;
      case UIIcon::Book: return Book24Icon;
      case UIIcon::File: return File24Icon;
      default: return nullptr;
    }
  } else if (size == 32) {
    switch (icon) {
      case UIIcon::Folder: return FolderIcon;
      case UIIcon::Book: return BookIcon;
      case UIIcon::Recent: return RecentIcon;
      case UIIcon::Settings: return Settings2Icon;
      case UIIcon::Transfer: return TransferIcon;
      case UIIcon::Library: return LibraryIcon;
      case UIIcon::Wifi: return WifiIcon;
      case UIIcon::Hotspot: return HotspotIcon;
      default: return nullptr;
    }
  }
  return nullptr;
}
} // namespace

namespace CustomMetrics {
const ThemeMetrics values = {.batteryWidth = 15,
                             .batteryHeight = 12,
                             .topPadding = 5,
                             .batteryBarHeight = 20,
                             .headerHeight = 50,
                             .verticalSpacing = 12,
                             .contentSidePadding = 25,
                             .listRowHeight = 40,
                             .listWithSubtitleRowHeight = 70,
                             .menuRowHeight = 52,
                             .menuSpacing = 8,
                             .tabSpacing = 12,
                             .tabBarHeight = 55,
                             .scrollBarWidth = 4,
                             .scrollBarRightOffset = 5,
                             .homeTopPadding = 40,
                             .homeCoverHeight = 320,
                             .homeCoverTileHeight = 420,
                             .homeRecentBooksCount = 1,
                             .buttonHintsHeight = 40,
                             .sideButtonHintsWidth = 30,
                             .progressBarHeight = 16,
                             .bookProgressBarHeight = 4,
                             .keyboardKeyWidth = 22,
                             .keyboardKeyHeight = 30,
                             .keyboardKeySpacing = 10,
                             .keyboardBottomAligned = false,
                             .keyboardCenteredText = true};
}

void CustomTheme::drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle) const {
  const auto& metrics = CustomMetrics::values;

  // Clear battery area
  constexpr int maxBatteryWidth = 85;
  renderer.fillRect(rect.x + rect.width - maxBatteryWidth, rect.y + 5, maxBatteryWidth, metrics.batteryHeight + 10,
                    false);

  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  const int batteryX = rect.x + rect.width - 15 - metrics.batteryWidth;

  drawBatteryRight(renderer, Rect{batteryX, rect.y + 8, metrics.batteryWidth, metrics.batteryHeight},
                   showBatteryPercentage);

  if (title) {
    int padding = rect.width - batteryX + metrics.batteryWidth;
    auto truncatedTitle = renderer.truncatedText(UI_12_FONT_ID, title,
                                                 rect.width - padding * 2 - metrics.contentSidePadding * 2,
                                                 EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, metrics.contentSidePadding, rect.y + 8, truncatedTitle.c_str(), true,
                      EpdFontFamily::BOLD);
  }
}

void CustomTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                 const std::function<std::string(int index)>& buttonLabel,
                                 const std::function<UIIcon(int index)>& rowIcon) const {
  const auto& metrics = CustomMetrics::values;
  const int cornerRadius = metrics.menuRowHeight / 2;

  for (int i = 0; i < buttonCount; ++i) {
    const int tileY = rect.y + i * (metrics.menuRowHeight + metrics.menuSpacing);
    const bool selected = (selectedIndex == i);

    std::string label = buttonLabel(i);
    // Increased font to UI_12
    const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, label.c_str(), EpdFontFamily::BOLD);
    const int textHeight = renderer.getTextHeight(UI_12_FONT_ID);

    int boxX = rect.x + metrics.contentSidePadding;
    int boxWidth = textWidth + 44; // Slightly more padding for larger font
    
    if (boxWidth > rect.width - metrics.contentSidePadding * 2) {
        boxWidth = rect.width - metrics.contentSidePadding * 2;
    }

    if (selected) {
      renderer.fillRoundedRect(boxX, tileY, boxWidth, metrics.menuRowHeight, cornerRadius, Black);
    }

    const int textX = boxX + 22; 
    const int textY = tileY + (metrics.menuRowHeight - textHeight) / 2;

    renderer.drawText(UI_12_FONT_ID, textX, textY, label.c_str(), !selected, EpdFontFamily::BOLD);
  }
}

void CustomTheme::drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                           const std::function<std::string(int index)>& rowTitle,
                           const std::function<std::string(int index)>& rowSubtitle,
                           const std::function<UIIcon(int index)>& rowIcon,
                           const std::function<std::string(int index)>& rowValue, bool highlightValue) const {
  const auto& metrics = CustomMetrics::values;
  int rowHeight = (rowSubtitle != nullptr) ? metrics.listWithSubtitleRowHeight : metrics.listRowHeight;
  int pageItems = rect.height / rowHeight;

  const auto pageStartIndex = selectedIndex / pageItems * pageItems;
  for (int i = pageStartIndex; i < itemCount && i < pageStartIndex + pageItems; i++) {
    const int itemY = rect.y + (i % pageItems) * rowHeight;
    const bool selected = (i == selectedIndex);

    int contentX = rect.x + metrics.contentSidePadding;
    int contentWidth = rect.width - metrics.contentSidePadding * 2;
    int availableWidth = contentWidth;
    int textX = contentX;

    // 1. Icon Support
    int iconSize = (rowSubtitle != nullptr) ? 32 : 24;
    if (rowIcon != nullptr) {
      UIIcon icon = rowIcon(i);
      const uint8_t* iconBitmap = iconForName(icon, iconSize);
      if (iconBitmap != nullptr) {
          int iconY = itemY + (rowHeight - iconSize) / 2;
          renderer.drawIcon(iconBitmap, textX, iconY, iconSize, iconSize);
          textX += iconSize + 12;
          availableWidth -= (iconSize + 12);
      }
    }

    // 2. Value Support (Settings)
    std::string valueText = "";
    int valueWidth = 0;
    if (rowValue != nullptr) {
        valueText = rowValue(i);
        if (!valueText.empty()) {
            valueWidth = renderer.getTextWidth(UI_10_FONT_ID, valueText.c_str());
            availableWidth -= (valueWidth + 20);
        }
    }

    std::string title = rowTitle(i);
    auto titleFont = UI_12_FONT_ID; 
    auto fontStyle = selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;

    // 3. Selection Capsule
    if (selected) {
        if (rowValue != nullptr) {
            // Settings: Full-width capsule bar
            renderer.fillRoundedRect(contentX - 15, itemY + 4, contentWidth + 30, rowHeight - 8, (rowHeight - 8) / 2, Black);
        } else {
            // Files: Dynamic capsule fitting title text
            const int textWidth = renderer.getTextWidth(titleFont, title.c_str(), fontStyle);
            int capsuleWidth = textWidth + 40;
            if (capsuleWidth > availableWidth + 20) capsuleWidth = availableWidth + 20;
            renderer.fillRoundedRect(textX - 20, itemY + 4, capsuleWidth, rowHeight - 8, (rowHeight - 8) / 2, Black);
        }
    }

    // 4. Draw Title
    auto truncatedTitle = renderer.truncatedText(titleFont, title.c_str(), availableWidth, fontStyle);
    renderer.drawText(titleFont, textX, itemY + (rowSubtitle ? 5 : (rowHeight - renderer.getTextHeight(titleFont))/2), 
                      truncatedTitle.c_str(), !selected, fontStyle);

    // 5. Draw Value (Right-aligned)
    if (!valueText.empty()) {
        int vX = rect.x + rect.width - metrics.contentSidePadding - valueWidth;
        int vY = itemY + (rowHeight - renderer.getTextHeight(UI_10_FONT_ID))/2;
        renderer.drawText(UI_10_FONT_ID, vX, vY, valueText.c_str(), !selected);
    }

    // 6. Subtitle support
    if (rowSubtitle != nullptr) {
      std::string subtitle = rowSubtitle(i);
      auto truncatedSubtitle = renderer.truncatedText(UI_10_FONT_ID, subtitle.c_str(), availableWidth);
      renderer.drawText(UI_10_FONT_ID, textX, itemY + 35, truncatedSubtitle.c_str(), !selected);
    }
  }
}

void CustomTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                      const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                      bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const auto& metrics = CustomMetrics::values;
  const bool hasBook = !recentBooks.empty();
  const bool selected = (hasBook && selectorIndex == 0);

  if (hasBook) {
    // 1. Labels at the TOP (Centered)
    std::string title = recentBooks[0].title;
    std::string author = recentBooks[0].author;

    int textY = rect.y;
    int maxTextWidth = rect.width - 40;

    auto truncTitle = renderer.truncatedText(UI_12_FONT_ID, title.c_str(), maxTextWidth, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_12_FONT_ID, textY, truncTitle.c_str(), true, EpdFontFamily::BOLD);

    if (!author.empty()) {
        auto truncAuthor = renderer.truncatedText(UI_10_FONT_ID, author.c_str(), maxTextWidth);
        renderer.drawCenteredText(UI_10_FONT_ID, textY + 28, truncAuthor.c_str(), true);
    }

    // 2. Cover BELOW labels
    int containerHeight = metrics.homeCoverHeight;
    int containerWidth = rect.width - metrics.contentSidePadding * 2;
    int bookY = rect.y + 70; // Slightly more offset

    if (!recentBooks[0].coverBmpPath.empty() && !coverRendered) {
      const std::string coverBmpPath =
          UITheme::getCoverThumbPath(recentBooks[0].coverBmpPath, metrics.homeCoverHeight);

      FsFile file;
      if (Storage.openFileForRead("HOME", coverBmpPath, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          int bookWidth = containerWidth;
          if (bitmap.getHeight() > 0) {
              bookWidth = (bitmap.getWidth() * containerHeight) / bitmap.getHeight();
          }
          if (bookWidth > containerWidth) bookWidth = containerWidth;
          
          int bookX = rect.x + (rect.width - bookWidth) / 2;

          if (selected) {
              // Selection Frame - Rounded outer ring, but no inner boarder around the BMP itself
              renderer.drawRoundedRect(bookX - 4, bookY - 4, bookWidth + 8, containerHeight + 8, 2, 16, true);
          }

          renderer.drawBitmap(bitmap, bookX, bookY, bookWidth, containerHeight);
          // Removed the inner drawRoundedRect boarder around bitmap
          
          coverBufferStored = storeCoverBuffer();
          coverRendered = true;
        }
        file.close();
      }
    }

    if (!coverRendered && !bufferRestored) {
        int bookWidth = containerWidth / 2;
        int bookX = rect.x + (rect.width - bookWidth) / 2;
        if (selected) {
           renderer.drawRoundedRect(bookX - 4, bookY - 4, bookWidth + 8, containerHeight + 8, 2, 16, true);
        }
        renderer.drawRoundedRect(bookX, bookY, bookWidth, containerHeight, 1, 16, true);
        renderer.drawCenteredText(UI_10_FONT_ID, bookY + containerHeight / 2, "No Cover", true);
    }
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, rect.y + rect.height / 3, tr(STR_NO_RECENT_BOOKS), true);
  }
}

void CustomTheme::drawProgressBar(const GfxRenderer& renderer, Rect rect, size_t current, size_t total) const {
    if (total == 0) return;
    int percent = (current * 100) / total;

    // Rounded progress bar
    int r = rect.height / 2;
    renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, r, true);

    int fillW = (rect.width - 4) * percent / 100;
    if (fillW > r * 2) {
        renderer.fillRoundedRect(rect.x + 2, rect.y + 2, fillW, rect.height - 4, r - 2, Black);
    } else if (fillW > 0) {
        renderer.fillRect(rect.x + 2, rect.y + 2, fillW, rect.height - 4, Black);
    }
}
