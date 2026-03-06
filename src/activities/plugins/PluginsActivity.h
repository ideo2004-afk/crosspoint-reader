#pragma once
#include <functional>
#include <vector>

#include "../Activity.h"
#include "components/themes/BaseTheme.h"

class PluginsActivity final : public Activity {
 private:
  int menuSelectorIndex = 0;
  bool skipNextButtonCheck = false;
  const std::function<void()> onGoHome;
  const std::function<void()> onFlashcardOpen;
  const std::function<void()> onQubicOpen;

 public:
  explicit PluginsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                           const std::function<void()>& onGoHome,
                           const std::function<void()>& onFlashcardOpen,
                           const std::function<void()>& onQubicOpen)
      : Activity("Plugins", renderer, mappedInput),
        onGoHome(onGoHome),
        onFlashcardOpen(onFlashcardOpen),
        onQubicOpen(onQubicOpen) {}

  void onEnter() override;
  void loop() override;
  void render(Activity::RenderLock&&) override;
};
