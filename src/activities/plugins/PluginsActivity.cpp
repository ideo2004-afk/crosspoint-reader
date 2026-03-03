#include "PluginsActivity.h"
#include "components/UITheme.h"
#include "I18n.h"

void PluginsActivity::onEnter() {
  Activity::onEnter();
  menuSelectorIndex = 0;
  requestUpdate();
}

void PluginsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (menuSelectorIndex > 0) {
      menuSelectorIndex--;
      requestUpdate();
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (menuSelectorIndex < 1) { // 2 items for now
      menuSelectorIndex++;
      requestUpdate();
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (menuSelectorIndex == 0) {
      onFlashcardOpen();
    } else if (menuSelectorIndex == 1) {
      onQubicOpen();
    }
  }
}

void PluginsActivity::render(Activity::RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "App Plugins");

  std::vector<const char*> menuItems = {"Flashcards", "3D Tic-Tac-Toe"};
  std::vector<UIIcon> menuIcons = {Library, Game};

  GUI.drawButtonMenu(
      renderer,
      Rect{0, metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.headerHeight + metrics.verticalSpacing)},
      static_cast<int>(menuItems.size()), menuSelectorIndex,
      [&menuItems](int index) { return std::string(menuItems[index]); },
      [&menuIcons](int index) { return menuIcons[index]; });

  renderer.displayBuffer();
}
