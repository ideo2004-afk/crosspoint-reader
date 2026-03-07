#include "NetworkModeSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int MENU_ITEM_COUNT = 5;
}  // namespace

void NetworkModeSelectionActivity::onEnter() {
  Activity::onEnter();

  // Reset selection
  selectedIndex = 0;
  skipNextButtonCheck = true;
  requestUpdate();
}

void NetworkModeSelectionActivity::onExit() { Activity::onExit(); }

void NetworkModeSelectionActivity::loop() {
  if (skipNextButtonCheck) {
    if (!mappedInput.isAnyPressed() && !mappedInput.wasAnyReleased()) {
      skipNextButtonCheck = false;
    }
    return;
  }

  // Handle back button - cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
    return;
  }

  // Handle confirm button - select current option
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selectedIndex == 0) {
      onModeSelected(NetworkMode::JOIN_NETWORK);
    } else if (selectedIndex == 1) {
      onModeSelected(NetworkMode::CONNECT_CALIBRE);
    } else if (selectedIndex == 2) {
      onModeSelected(NetworkMode::CREATE_HOTSPOT);
    } else if (selectedIndex == 3) {
      onFlashcard();
    } else if (selectedIndex == 4) {
      onQubic();
    }
    return;
  }

  // Handle navigation
  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, MENU_ITEM_COUNT);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, MENU_ITEM_COUNT);
    requestUpdate();
  });
}

void NetworkModeSelectionActivity::render(Activity::RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Toolbox");

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  // Menu items and descriptions
  auto rowTitle = [](int index) {
      static const char* titles[] = {
          I18N.get(StrId::STR_JOIN_NETWORK),
          I18N.get(StrId::STR_CALIBRE_WIRELESS),
          I18N.get(StrId::STR_CREATE_HOTSPOT),
          "Flashcards",
          "3D Tic-Tac-Toe"
      };
      return std::string(titles[index]);
  };

  auto rowDesc = [](int index) {
      static const char* descs[] = {
          I18N.get(StrId::STR_JOIN_DESC),
          I18N.get(StrId::STR_CALIBRE_DESC),
          I18N.get(StrId::STR_HOTSPOT_DESC),
          "Study your flashcards",
          "3D board game"
      };
      return std::string(descs[index]);
  };

  auto rowIcon = [](int index) {
      static const UIIcon icons[] = {
          UIIcon::Wifi,
          UIIcon::Library,
          UIIcon::Hotspot,
          UIIcon::Book,
          UIIcon::Game
      };
      return icons[index];
  };

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(MENU_ITEM_COUNT),
               selectedIndex, rowTitle, rowDesc, rowIcon);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
