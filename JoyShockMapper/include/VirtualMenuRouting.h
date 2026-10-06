#pragma once

#include "TouchGridRouting.h"
#include <map>
#include <string>

// Pure native selection state. It previews an item independently of applying
// its Mapping, and never needs renderer or overlay input to function.
enum class VirtualMenuType { RADIAL, TOUCH, HOTBAR };
enum class VirtualMenuActivation { HOLD, TOGGLE, ALWAYS, COMMAND };
enum class VirtualMenuSelection { CLICK, TOUCH_RELEASE, ACTIVATION_RELEASE, CONTINUOUS };

// Per-controller commands share a menu, but each held binding owns its release.
struct VirtualMenuCommandState {
  bool latched = false;
  unsigned cancellation = 0;
  std::map<int, int> holders;
  bool active() const { return latched || !holders.empty(); }
  void apply(const std::string &verb, int owner, bool release = false) {
    if (verb == "HOLD") {
      if (!release) ++holders[owner];
      else { auto it = holders.find(owner); if (it != holders.end() && --it->second == 0) holders.erase(it); }
    } else if (!release) {
      if (verb == "OPEN") latched = true;
      else if (verb == "CLOSE" || (verb == "TOGGLE" && active())) {
        latched = false; holders.clear(); ++cancellation;
      } else if (verb == "TOGGLE") latched = true;
    }
  }
};

struct VirtualMenuInput {
  bool activation = false, contact = false, confirm = false, cancel = false;
  bool digitalNavigation = false;
  bool previous = false, next = false;
  bool atRest = false; // Stick centre, distinct from ending its deflection gesture.
  bool joystickCursor = false;
  float x = .5f, y = .5f;
};
struct VirtualMenuResult {
  bool open = false;
  bool navigating = false;
  int selected = -1, pulse = -1, held = -1;
  bool cursor = false;
  float x = .5f, y = .5f;
};
struct VirtualMenuRouting {
  bool open = false, blocked = false;
  bool lastActivation = false, lastContact = false, lastConfirm = false;
  bool lastPrevious = false, lastNext = false;
  int selected = -1, remembered = 0;

  // One native source has one owner. A fresh activation takes precedence over
  // an existing menu; ALWAYS is a fallback. Retain ownership for the closing
  // frame so a normal release can commit without leaking to the fallback.
  int ownershipPriority(VirtualMenuActivation activation, bool pressed) const {
    if (activation != VirtualMenuActivation::ALWAYS && pressed && !lastActivation && !blocked) return 4;
    if (open) return 3;
    if (activation == VirtualMenuActivation::HOLD && pressed && !blocked) return 2;
    return activation == VirtualMenuActivation::ALWAYS ? 1 : 0;
  }

  VirtualMenuResult update(VirtualMenuType type, VirtualMenuActivation activation,
    VirtualMenuSelection selection, int count, int columns, float deadzone,
    const VirtualMenuInput &input, bool centerAction = false) {
    const bool activationRise = input.activation && !lastActivation;
    const bool activationFall = !input.activation && lastActivation;
    if (!input.activation) blocked = false;
    bool active = activation == VirtualMenuActivation::ALWAYS ||
      (activation == VirtualMenuActivation::HOLD ? input.activation && !blocked : open);
    if (activation == VirtualMenuActivation::TOGGLE && activationRise) active = !open;
    VirtualMenuResult result;
    if (count < 1 || count > 25 || columns < 1 || columns > 25 ||
      !std::isfinite(deadzone) || deadzone < 0.f || deadzone >= 1.f) active = false;
    const bool newlyOpen = active && !open;
    if (newlyOpen) selected = type == VirtualMenuType::HOTBAR ? std::clamp(remembered, 0, count - 1) : -1;
    const int previousSelection = selected;
    // Cursor navigation follows position even on the closing poll. Returning
    // to rest must clear a stale segment before activation release commits it.
    if (input.joystickCursor && (active || open) && type != VirtualMenuType::HOTBAR) {
      selected = type == VirtualMenuType::RADIAL
        ? touchRadialCell(true, input.x, input.y, count, deadzone)
        : input.atRest ? -1 : touchGridCell(true, input.x, input.y, columns, (count + columns - 1) / columns);
      if (selected >= count) selected = -1;
      if (type == VirtualMenuType::RADIAL && input.atRest) selected = centerAction ? count : -1;
    }
    const int before = selected;
    if (active && !input.cancel) {
      if (type == VirtualMenuType::HOTBAR) {
        if (input.previous && !lastPrevious) selected = (selected + count - 1) % count;
        if (input.next && !lastNext) selected = (selected + 1) % count;
        remembered = selected;
      } else if (!input.joystickCursor && (input.contact || (type == VirtualMenuType::RADIAL && centerAction && input.atRest))) {
        selected = type == VirtualMenuType::RADIAL
          ? touchRadialCell(true, input.x, input.y, count, deadzone)
          : touchGridCell(true, input.x, input.y, columns, (count + columns - 1) / columns);
        if (selected >= count) selected = -1;
        if (type == VirtualMenuType::RADIAL && centerAction && std::isfinite(input.x) && std::isfinite(input.y) &&
          std::hypot(input.x - .5f, input.y - .5f) <= deadzone * .5f) selected = count;
      }
      // Contact ending retains the last valid preview for release selection.
      if (selection == VirtualMenuSelection::CLICK && input.confirm && !lastConfirm)
        result.pulse = selected;
      if (selection == VirtualMenuSelection::TOUCH_RELEASE && lastContact && !input.contact && !newlyOpen)
        result.pulse = input.joystickCursor ? previousSelection : before;
      if (selection == VirtualMenuSelection::CONTINUOUS && input.contact)
        result.held = selected;
    }
    if (open && !active && selection == VirtualMenuSelection::ACTIVATION_RELEASE &&
      activation != VirtualMenuActivation::ALWAYS && (activationFall || activationRise)) result.pulse = before;
    if (open && !active && selection == VirtualMenuSelection::TOUCH_RELEASE && lastContact && !input.contact)
      result.pulse = input.joystickCursor ? previousSelection : before;
    if (input.cancel) { active = false; blocked = input.activation; result.pulse = result.held = -1; }
    open = active;
    if (!open) selected = -1;
    result.open = open; result.selected = selected;
    result.navigating = open && (input.digitalNavigation ? input.previous || input.next : input.contact);
    result.cursor = open && input.joystickCursor;
    result.x = input.x; result.y = input.y;
    lastActivation = input.activation; lastContact = input.contact; lastConfirm = input.confirm;
    lastPrevious = input.previous; lastNext = input.next;
    return result;
  }
};
