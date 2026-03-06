#include "MappedInputManager.h"

#include "CrossPointSettings.h"

namespace {
using ButtonIndex = uint8_t;

struct SideLayoutMap {
  ButtonIndex pageBack;
  ButtonIndex pageForward;
};

// Order matches CrossPointSettings::SIDE_BUTTON_LAYOUT.
constexpr SideLayoutMap kSideLayouts[] = {
    {HalGPIO::BTN_UP, HalGPIO::BTN_DOWN},
    {HalGPIO::BTN_DOWN, HalGPIO::BTN_UP},
};
}  // namespace

bool MappedInputManager::mapButton(const Button button, bool (HalGPIO::*fn)(uint8_t) const) const {
  const auto sideLayout = static_cast<CrossPointSettings::SIDE_BUTTON_LAYOUT>(SETTINGS.sideButtonLayout);
  const auto& side = kSideLayouts[sideLayout];

  switch (button) {
    case Button::Back:
      return (gpio.*fn)(SETTINGS.frontButtonBack);
    case Button::Confirm:
      return (gpio.*fn)(SETTINGS.frontButtonConfirm);
    case Button::Left:
      return (gpio.*fn)(SETTINGS.frontButtonLeft);
    case Button::Right:
      return (gpio.*fn)(SETTINGS.frontButtonRight);
    case Button::Up:
      return (gpio.*fn)(HalGPIO::BTN_UP);
    case Button::Down:
      return (gpio.*fn)(HalGPIO::BTN_DOWN);
    case Button::Power:
      return (gpio.*fn)(HalGPIO::BTN_POWER);
    case Button::PageBack:
      return (gpio.*fn)(side.pageBack);
    case Button::PageForward:
      return (gpio.*fn)(side.pageForward);
  }

  return false;
}

void MappedInputManager::update() {
  gpio.update();
  // Clear masks for any buttons released or not pressed
  for (int i = 0; i < 16; i++) {
    // Logical buttons
    if (i <= static_cast<int>(Button::PageForward)) {
      if (!isPressed(static_cast<Button>(i))) {
        firedLongPressMask &= ~(1 << i);
      }
    }
    // Physical buttons (BTN_UP is 4, BTN_DOWN is 5, etc. from InputManager.h)
    if (!gpio.isPressed(i)) {
      firedLongPressRawMask &= ~(1 << i);
    }
  }
}

bool MappedInputManager::wasPressed(const Button button) const { return mapButton(button, &HalGPIO::wasPressed); }

bool MappedInputManager::wasReleased(const Button button) const { 
  if (mapButton(button, &HalGPIO::wasReleased)) {
    int bit = static_cast<int>(button);
    if (ignoreNextReleaseMask & (1 << bit)) {
      ignoreNextReleaseMask &= ~(1 << bit);
      return false;
    }
    return true;
  }
  return false;
}

bool MappedInputManager::isPressed(const Button button) const { return mapButton(button, &HalGPIO::isPressed); }

bool MappedInputManager::isAnyPressed() const { return gpio.isAnyPressed(); }

bool MappedInputManager::wasAnyPressed() const { return gpio.wasAnyPressed(); }

bool MappedInputManager::wasAnyReleased() const { return gpio.wasAnyReleased(); }

unsigned long MappedInputManager::getHeldTime() const { return gpio.getHeldTime(); }

bool MappedInputManager::isLongPressed(const Button button, const unsigned long threshold) const {
  return isPressed(button) && getHeldTime() >= threshold;
}

bool MappedInputManager::wasLongPressed(const Button button, const unsigned long threshold) {
  if (isLongPressed(button, threshold)) {
    int bit = static_cast<int>(button);
    if (!(firedLongPressMask & (1 << bit))) {
      firedLongPressMask |= (1 << bit);
      ignoreNextReleaseMask |= (1 << bit);
      
      // Also protect the physical buttons in this cluster
      if (button == Button::Back) {
        ignoreNextReleaseRawMask |= (1 << HalGPIO::BTN_BACK) | (1 << HalGPIO::BTN_CONFIRM);
      } else if (button == Button::Confirm) {
        ignoreNextReleaseRawMask |= (1 << HalGPIO::BTN_LEFT) | (1 << HalGPIO::BTN_RIGHT);
      } else if (button == Button::Up) {
        ignoreNextReleaseRawMask |= (1 << HalGPIO::BTN_UP);
      } else if (button == Button::Down) {
        ignoreNextReleaseRawMask |= (1 << HalGPIO::BTN_DOWN);
      }
      return true;
    }
  }
  return false;
}

bool MappedInputManager::wasShortPressed(const Button button, const unsigned long threshold) const {
  return wasReleased(button) && getHeldTime() < threshold;
}

MappedInputManager::Labels MappedInputManager::mapLabels(const char* back, const char* confirm, const char* previous,
                                                         const char* next) const {
  // Build the label order based on the configured hardware mapping.
  auto labelForHardware = [&](uint8_t hw) -> const char* {
    if (hw == SETTINGS.frontButtonBack) return back;
    if (hw == SETTINGS.frontButtonConfirm) return confirm;
    if (hw == SETTINGS.frontButtonLeft) return previous;
    if (hw == SETTINGS.frontButtonRight) return next;
    return "";
  };

  return {labelForHardware(HalGPIO::BTN_BACK), labelForHardware(HalGPIO::BTN_CONFIRM),
          labelForHardware(HalGPIO::BTN_LEFT), labelForHardware(HalGPIO::BTN_RIGHT)};
}

int MappedInputManager::getPressedFrontButton() const {
  if (gpio.wasPressed(HalGPIO::BTN_BACK)) return HalGPIO::BTN_BACK;
  if (gpio.wasPressed(HalGPIO::BTN_CONFIRM)) return HalGPIO::BTN_CONFIRM;
  if (gpio.wasPressed(HalGPIO::BTN_LEFT)) return HalGPIO::BTN_LEFT;
  if (gpio.wasPressed(HalGPIO::BTN_RIGHT)) return HalGPIO::BTN_RIGHT;
  return -1;
}

bool MappedInputManager::wasReleasedRaw(uint8_t buttonIndex) const { 
  if (gpio.wasReleased(buttonIndex)) {
    if (ignoreNextReleaseRawMask & (1 << buttonIndex)) {
      ignoreNextReleaseRawMask &= ~(1 << buttonIndex);
      return false;
    }
    return true;
  }
  return false;
}

bool MappedInputManager::wasPressedRaw(uint8_t buttonIndex) const { return gpio.wasPressed(buttonIndex); }

bool MappedInputManager::isPressedRaw(uint8_t buttonIndex) const { return gpio.isPressed(buttonIndex); }

bool MappedInputManager::wasReleasedAnyOf(uint8_t a, uint8_t b) const {
  bool releasedA = wasReleasedRaw(a);
  bool releasedB = wasReleasedRaw(b);
  return releasedA || releasedB;
}

bool MappedInputManager::isPressedAnyOf(uint8_t a, uint8_t b) const {
  return gpio.isPressed(a) || gpio.isPressed(b);
}

bool MappedInputManager::wasLongPressedRaw(uint8_t buttonIndex, unsigned long threshold) {
  if (gpio.isPressed(buttonIndex) && gpio.getHeldTime() >= threshold) {
    if (!(firedLongPressRawMask & (1 << buttonIndex))) {
      firedLongPressRawMask |= (1 << buttonIndex);
      ignoreNextReleaseRawMask |= (1 << buttonIndex);
      return true;
    }
  }
  return false;
}

bool MappedInputManager::wasShortPressedRaw(uint8_t buttonIndex, unsigned long threshold) const {
  return wasReleasedRaw(buttonIndex) && gpio.getHeldTime() < threshold;
}

void MappedInputManager::ignoreNextReleaseRaw(uint8_t buttonIndex) {
  if (buttonIndex < 16) {
    ignoreNextReleaseRawMask |= (1 << buttonIndex);
  }
}

void MappedInputManager::consumeButtonRaw(uint8_t buttonIndex) {
  if (buttonIndex < 16) {
    ignoreNextReleaseRawMask |= (1 << buttonIndex);
    firedLongPressRawMask |= (1 << buttonIndex);
  }
}