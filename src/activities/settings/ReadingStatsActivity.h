#pragma once

#include <functional>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
#include "ReadingStatsStore.h"

class ReadingStatsActivity final : public Activity {
  const std::function<void()> onExitCallback;
  ButtonNavigator buttonNavigator;
  
  std::vector<BookStats> topBooks;
  int selectedIndex = 0;
  int bookCount = 0;

 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                const std::function<void()>& onExitCallback)
      : Activity("ReadingStats", renderer, mappedInput), onExitCallback(onExitCallback) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(Activity::RenderLock&&) override;
};
