/**
 * XtcReaderActivity.h
 *
 * XTC ebook reader activity for CrossPoint Reader
 * Displays pre-rendered XTC pages on e-ink display
 */

#pragma once

#include <Xtc.h>

#include "activities/ActivityWithSubactivity.h"

class XtcReaderActivity final : public ActivityWithSubactivity {
  std::shared_ptr<Xtc> xtc;

  uint32_t currentPage = 0;
  int pagesUntilFullRefresh = 0;
  uint32_t sessionStartMillis = 0;

  const std::function<void()> onGoBack;
  const std::function<void()> onGoHome;

  bool inMenu = false;
  int menuSelectedIndex = 0;
  bool pendingScreenshot = false;

  std::vector<uint32_t> bookmarks;
  void renderPage();
  void renderStatusBar() const;
  void renderBookmarkIndicator() const;
  void renderMenu() const;
  void saveProgress() const;
  void loadProgress();
  void saveBookmarks() const;
  void loadBookmarks();
  void toggleBookmark();
  void nextBookmark();
  bool isPageBookmarked(uint32_t page) const;
  void jumpPercent(int deltaPercent);

 public:
  explicit XtcReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::unique_ptr<Xtc> xtc,
                             const std::function<void()>& onGoBack, const std::function<void()>& onGoHome)
      : ActivityWithSubactivity("XtcReader", renderer, mappedInput),
        xtc(std::move(xtc)),
        onGoBack(onGoBack),
        onGoHome(onGoHome) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(Activity::RenderLock&&) override;
};
