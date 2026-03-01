#include "RecentBooksActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/StringUtils.h"
#include <Epub.h>
#include <Xtc.h>
#include "components/icons/book.h"

namespace {
constexpr unsigned long GO_HOME_MS = 1000;
}  // namespace

void RecentBooksActivity::loadRecentBooks() {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min((size_t)9, books.size()));

  for (const auto& book : books) {
    if (recentBooks.size() >= 9) break;
    // Skip if file no longer exists
    if (!Storage.exists(book.path.c_str())) {
      continue;
    }
    recentBooks.push_back(book);
  }
}

void RecentBooksActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        if (StringUtils::checkFileExtension(book.path, ".epub")) {
          // If epub, try to load the metadata for title/author and cover
          Epub epub(book.path, "/.crosspoint");
          epub.load(false, true); // Skip loading css since we only need metadata here

          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          requestUpdate();
        } else if (StringUtils::checkFileExtension(book.path, ".xtch") ||
                   StringUtils::checkFileExtension(book.path, ".xtc")) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void RecentBooksActivity::onEnter() {
  Activity::onEnter();

  // Load data
  loadRecentBooks();

  selectorIndex = 0;
  requestUpdate();
}

void RecentBooksActivity::onExit() {
  Activity::onExit();
  recentBooks.clear();
}

void RecentBooksActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!recentBooks.empty() && selectorIndex < static_cast<int>(recentBooks.size())) {
      LOG_DBG("RBA", "Selected recent book: %s", recentBooks[selectorIndex].path.c_str());
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }

  int listSize = static_cast<int>(recentBooks.size());

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  // Fast forward jumps by a row (3 items)
  buttonNavigator.onNextContinuous([this, listSize] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, 3);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, 3);
    requestUpdate();
  });
}

void RecentBooksActivity::render(Activity::RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MENU_RECENT_BOOKS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int gridTopOffset = 20;
  
  // Calculate grid layout sizes
  int columns = 3;
  int rows = 3;
  
  // Hardcoded for 3x3 layout fit (e.g. 1404x1872 standard res aspect ratio)
  int coverWidth = (pageWidth - (metrics.contentSidePadding * 2) - (metrics.verticalSpacing * (columns - 1))) / columns;
  // Preserve rough 3:4 aspect ratio for covers
  int coverHeight = (coverWidth * 4) / 3;

  // Recent tab
  if (recentBooks.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_RECENT_BOOKS));
  } else {
    for (size_t i = 0; i < recentBooks.size(); ++i) {
      if (i >= 9) break;

      int col = i % columns;
      int row = i / columns;
      int x = metrics.contentSidePadding + (col * (coverWidth + metrics.verticalSpacing));
      int y = contentTop + gridTopOffset + (row * (coverHeight + metrics.verticalSpacing));

      Rect coverRect(x, y, coverWidth, coverHeight);

      // Draw cover image or fallback icon
      if (!recentBooks[i].coverBmpPath.empty()) {
        std::string coverPath = UITheme::getCoverThumbPath(recentBooks[i].coverBmpPath, coverHeight);
        if (Storage.exists(coverPath.c_str())) {
          FsFile file;
          if (Storage.openFileForRead("HOME", coverPath, file)) {
            Bitmap bmp(file);
            if (bmp.parseHeaders() == BmpReaderError::Ok) {
               renderer.drawBitmap(bmp, x + (coverWidth - bmp.getWidth()) / 2, y + (coverHeight - bmp.getHeight()) / 2, bmp.getWidth(), bmp.getHeight());
            }
            file.close();
          }
        } else {
             renderer.drawIcon(BookIcon, x + (coverWidth-32)/2, y + (coverHeight-32)/2, 32, 32);
        }
      } else {
        renderer.drawIcon(BookIcon, x + (coverWidth-32)/2, y + (coverHeight-32)/2, 32, 32);
      }

      // Draw selection box around active element
      if (i == selectorIndex) {
        // drawRect takes a bool state for black (true) vs white (false)
        renderer.drawRect(coverRect.x - 4, coverRect.y - 4, coverRect.width + 8, coverRect.height + 8, true);
        renderer.drawRect(coverRect.x - 3, coverRect.y - 3, coverRect.width + 6, coverRect.height + 6, true);
        renderer.drawRect(coverRect.x - 5, coverRect.y - 5, coverRect.width + 10, coverRect.height + 10, true);
      }
    }
  }

  renderer.displayBuffer();

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(coverHeight);
  }
}
