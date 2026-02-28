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
      // Front LEFT cluster (BTN_BACK + BTN_CONFIRM) → logical Back
      return (gpio.*fn)(HalGPIO::BTN_BACK) || (gpio.*fn)(HalGPIO::BTN_CONFIRM);
    case Button::Confirm:
      // Front RIGHT cluster (BTN_LEFT + BTN_RIGHT) → logical Select/Confirm
      return (gpio.*fn)(HalGPIO::BTN_LEFT) || (gpio.*fn)(HalGPIO::BTN_RIGHT);
    case Button::Left:
      // Merged into Back cluster; no longer used as standalone logical button.
      return false;
    case Button::Right:
      // Merged into Confirm cluster; no longer used as standalone logical button.
      return false;
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
  // Clear long press flag for any logical buttons released this frame
  // Check each logical button
  for (int i = 0; i <= static_cast<int>(Button::PageForward); i++) {
    if (wasReleased(static_cast<Button>(i))) {
      firedLongPressMask &= ~(1 << i);
    }
  }
}

bool MappedInputManager::wasPressed(const Button button) const { return mapButton(button, &HalGPIO::wasPressed); }

bool MappedInputManager::wasReleased(const Button button) const { return mapButton(button, &HalGPIO::wasReleased); }

bool MappedInputManager::isPressed(const Button button) const { return mapButton(button, &HalGPIO::isPressed); }

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
    // Compare against configured logical roles and return the matching label.
    if (hw == SETTINGS.frontButtonBack) {
      return back;
    }
    if (hw == SETTINGS.frontButtonConfirm) {
      return confirm;
    }
    if (hw == SETTINGS.frontButtonLeft) {
      return previous;
    }
    if (hw == SETTINGS.frontButtonRight) {
      return next;
    }
    return "";
  };

  return {labelForHardware(HalGPIO::BTN_BACK), labelForHardware(HalGPIO::BTN_CONFIRM),
          labelForHardware(HalGPIO::BTN_LEFT), labelForHardware(HalGPIO::BTN_RIGHT)};
}

int MappedInputManager::getPressedFrontButton() const {
  // Scan the raw front buttons in hardware order.
  // This bypasses remapping so the remap activity can capture physical presses.
  if (gpio.wasPressed(HalGPIO::BTN_BACK)) {
    return HalGPIO::BTN_BACK;
  }
  if (gpio.wasPressed(HalGPIO::BTN_CONFIRM)) {
    return HalGPIO::BTN_CONFIRM;
  }
  if (gpio.wasPressed(HalGPIO::BTN_LEFT)) {
    return HalGPIO::BTN_LEFT;
  }
  if (gpio.wasPressed(HalGPIO::BTN_RIGHT)) {
    return HalGPIO::BTN_RIGHT;
  }
  return -1;
}

bool MappedInputManager::wasReleasedRaw(uint8_t buttonIndex) const { return gpio.wasReleased(buttonIndex); }

bool MappedInputManager::isPressedRaw(uint8_t buttonIndex) const { return gpio.isPressed(buttonIndex); }

bool MappedInputManager::wasReleasedAnyOf(uint8_t a, uint8_t b) const {
  return gpio.wasReleased(a) || gpio.wasReleased(b);
}

bool MappedInputManager::isPressedAnyOf(uint8_t a, uint8_t b) const {
  return gpio.isPressed(a) || gpio.isPressed(b);
}