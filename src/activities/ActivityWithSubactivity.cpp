#include "ActivityWithSubactivity.h"

#include <HalPowerManager.h>

void ActivityWithSubactivity::renderTaskLoop() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    {
      HalPowerManager::Lock powerLock;  // Ensure we don't go into low-power mode while rendering
      RenderLock lock(*this);
      if (!subActivity) {
        render(std::move(lock));
      }
      // If subActivity is set, consume the notification but skip parent render
      // Note: the sub-activity will call its render() from its own display task
    }
  }
}

void ActivityWithSubactivity::exitActivity() {
  // No need to lock, since onExit() already acquires its own lock
  if (subActivity) {
    LOG_DBG("ACT", "Exiting subactivity...");
    subActivity->onExit();
    subActivity.reset();
  }
}

void ActivityWithSubactivity::enterNewActivity(Activity* activity) {
  // Defer activity creation to the next loop iteration to prevent use-after-free
  // if the caller is currently executing from the current sub-activity's stack.
  pendingSubActivity = activity;
}

void ActivityWithSubactivity::loop() {
  if (pendingSubActivity) {
    Activity* act = pendingSubActivity;
    pendingSubActivity = nullptr;
    
    // Acquire lock and switch activity
    RenderLock lock(*this);
    exitActivity();
    subActivity.reset(act);
    subActivity->onEnter();
  }

  if (subActivity) {
    subActivity->loop();
  }
}

void ActivityWithSubactivity::requestUpdate() {
  if (!subActivity) {
    Activity::requestUpdate();
  }
  // Sub-activity should call their own requestUpdate() from their loop() function
}

void ActivityWithSubactivity::onExit() {
  // No need to lock, onExit() already acquires its own lock
  if (pendingSubActivity) {
    delete pendingSubActivity;
    pendingSubActivity = nullptr;
  }
  exitActivity();
  Activity::onExit();
}
