#pragma once

#include <HalGPIO.h>

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };

  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  explicit MappedInputManager(HalGPIO& gpio) : gpio(gpio) {}

  void update();
  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  bool wasAnyPressed() const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
  bool isLongPressed(Button button, unsigned long threshold = 500) const;
  bool wasLongPressed(Button button, unsigned long threshold = 500);
  bool wasShortPressed(Button button, unsigned long threshold = 500) const;
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const;
  // Returns the raw front button index that was pressed this frame (or -1 if none).
  int getPressedFrontButton() const;

  // Raw physical GPIO access (bypasses remapping) for activities with fixed layouts.
  bool wasReleasedRaw(uint8_t buttonIndex) const;
  bool isPressedRaw(uint8_t buttonIndex) const;
  bool wasReleasedAnyOf(uint8_t a, uint8_t b) const;
  bool isPressedAnyOf(uint8_t a, uint8_t b) const;

  bool wasLongPressedRaw(uint8_t buttonIndex, unsigned long threshold = 500);
  bool wasShortPressedRaw(uint8_t buttonIndex, unsigned long threshold = 500) const;

 private:
  HalGPIO& gpio;
  mutable uint16_t firedLongPressMask = 0;
  mutable uint16_t firedLongPressRawMask = 0;
  mutable uint16_t ignoreNextReleaseMask = 0;
  mutable uint16_t ignoreNextReleaseRawMask = 0;

  bool mapButton(Button button, bool (HalGPIO::*fn)(uint8_t) const) const;
};
