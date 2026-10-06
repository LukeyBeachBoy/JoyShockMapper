#include "VirtualMenuRouting.h"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
  VirtualMenuRouting state;
  VirtualMenuInput input;
  auto wheel = [&] { return state.update(VirtualMenuType::RADIAL, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::ACTIVATION_RELEASE, 8, 8, .2f, input); };
  assert(!wheel().open);
  input.activation = true; input.contact = true; input.x = .5f; input.y = .1f;
  auto result = wheel(); assert(result.open && result.selected == 0 && result.pulse == -1 && result.held == -1);
  input.x = .9f; input.y = .5f; result = wheel(); assert(result.selected == 2 && result.pulse == -1);
  input.activation = false; result = wheel(); assert(!result.open && result.pulse == 2);
  assert(wheel().pulse == -1); // one release, one output
  input.activation = true; input.x = input.y = .5f; assert(wheel().selected == -1);
  input.activation = false; assert(wheel().pulse == -1); // neutral cancels
  input.activation = true; input.y = .1f; assert(wheel().selected == 0);
  input.cancel = true; assert(!wheel().open); input.cancel = false; assert(!wheel().open);
  input.activation = false; assert(wheel().pulse == -1);
  input.activation = true; assert(wheel().open); // unblock only after activation release
  VirtualMenuRouting touch;
  input.activation = input.contact = true; input.x = .75f; input.y = .75f;
  auto grid = [&] { return touch.update(VirtualMenuType::TOUCH, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::TOUCH_RELEASE, 4, 2, .1f, input); };
  assert(grid().selected == 3); input.contact = false; assert(grid().pulse == 3); assert(grid().pulse == -1);
  VirtualMenuRouting click;
  input.contact = true; input.confirm = false;
  auto choose = [&] { return click.update(VirtualMenuType::TOUCH, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::CLICK, 4, 2, .1f, input); };
  assert(choose().pulse == -1); input.confirm = true; assert(choose().pulse == 3); assert(choose().pulse == -1);
  VirtualMenuRouting continuous;
  auto held = continuous.update(VirtualMenuType::TOUCH, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::CONTINUOUS, 4, 2, .1f, input);
  assert(held.held == 3 && held.pulse == -1);
  VirtualMenuRouting hotbar;
  input = {}; input.activation = true;
  auto bar = [&] { return hotbar.update(VirtualMenuType::HOTBAR, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::CLICK, 3, 3, 0.f, input); };
  assert(bar().selected == 0); input.previous = true; assert(bar().selected == 2); assert(bar().selected == 2);
  input.previous = false; input.next = true; assert(bar().selected == 0);
  input.activation = false; assert(!bar().open); input.activation = true; assert(bar().selected == 0);
  VirtualMenuRouting separate; input.next = false;
  assert(separate.update(VirtualMenuType::HOTBAR, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::CLICK, 3, 3, 0.f, input).selected == 0);
  VirtualMenuRouting toggle;
  input = {}; input.activation = true; input.contact = true; input.y = .1f;
  auto toggled = [&] { return toggle.update(VirtualMenuType::RADIAL, VirtualMenuActivation::TOGGLE,
    VirtualMenuSelection::ACTIVATION_RELEASE, 8, 8, .2f, input); };
  assert(toggled().open); input.activation = false; assert(toggled().open);
  input.activation = true; assert(toggled().pulse == 0 && !toggle.open);
  VirtualMenuRouting invalid;
  input.x = std::numeric_limits<float>::quiet_NaN();
  assert(invalid.update(VirtualMenuType::RADIAL, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::CLICK, 8, 8, .2f, input).selected == -1);
  assert(!invalid.update(VirtualMenuType::RADIAL, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::CLICK, 26, 8, .2f, input).open);
  VirtualMenuRouting fallback, foreground;
  assert(fallback.ownershipPriority(VirtualMenuActivation::ALWAYS, false) == 1);
  assert(foreground.ownershipPriority(VirtualMenuActivation::HOLD, true) == 4);
  input = {}; input.activation = input.contact = true; input.y = .1f;
  auto primary = [&] { return foreground.update(VirtualMenuType::RADIAL, VirtualMenuActivation::HOLD,
    VirtualMenuSelection::ACTIVATION_RELEASE, 8, 8, .2f, input); };
  assert(primary().open);
  assert(foreground.ownershipPriority(VirtualMenuActivation::HOLD, true) == 3);
  input.cancel = true;
  assert(primary().pulse == -1 && !foreground.open);
  assert(foreground.ownershipPriority(VirtualMenuActivation::HOLD, true) == 0);
  input.cancel = false;
  assert(!primary().open); // losing ownership cannot fire a release selection
  input.activation = false; assert(primary().pulse == -1);
  input.activation = true; assert(primary().open);
  input.activation = false;
  assert(foreground.ownershipPriority(VirtualMenuActivation::HOLD, false) == 3);
  assert(primary().pulse == 0); // closing frame beats ALWAYS fallback
  VirtualMenuRouting centered; input = {}; input.activation = input.contact = true;
  auto center = [&](VirtualMenuSelection selection) { return centered.update(VirtualMenuType::RADIAL, VirtualMenuActivation::HOLD, selection, 8, 8, .2f, input, true); };
  assert(center(VirtualMenuSelection::CLICK).selected == 8);
  input.confirm = true; assert(center(VirtualMenuSelection::CLICK).pulse == 8);
  assert(center(VirtualMenuSelection::CLICK).pulse == -1);
  input.confirm = false; assert(center(VirtualMenuSelection::CONTINUOUS).held == 8);
  input.contact = false; assert(center(VirtualMenuSelection::TOUCH_RELEASE).pulse == 8);
  input.atRest = true; assert(center(VirtualMenuSelection::CONTINUOUS).held == -1); // neutral stick is not held contact
  input.contact = true; input.atRest = false; input.y = .1f;
  assert(center(VirtualMenuSelection::TOUCH_RELEASE).selected == 0);
  input.contact = false; input.atRest = true; input.y = .5f;
  auto returning = center(VirtualMenuSelection::TOUCH_RELEASE);
  assert(returning.pulse == 0 && returning.selected == 8); // return commits prior segment
  input.activation = false; assert(center(VirtualMenuSelection::ACTIVATION_RELEASE).pulse == 8);
  // A cursor previews absolute position and clears on rest, including when
  // neutral and activation release arrive in the same hardware report.
  VirtualMenuRouting cursor; input = {}; input.joystickCursor = true;
  auto cursorWheel = [&](VirtualMenuSelection selection = VirtualMenuSelection::ACTIVATION_RELEASE, bool centerAction = false) {
    return cursor.update(VirtualMenuType::RADIAL, VirtualMenuActivation::HOLD, selection, 25, 25, .2f, input, centerAction);
  };
  input.activation = true; input.atRest = true;
  result = cursorWheel(); assert(result.cursor && result.selected == -1 && result.x == .5f && result.y == .5f);
  input.atRest = false; input.contact = true; input.y = .1f;
  assert(cursorWheel().selected == 0);
  input.atRest = true; input.contact = false; input.y = .5f;
  assert(cursorWheel().selected == -1);
  input.activation = false; assert(cursorWheel().pulse == -1);
  input.activation = input.contact = true; input.atRest = false; input.y = .1f;
  assert(cursorWheel().selected == 0);
  input.activation = input.contact = false; input.atRest = true; input.y = .5f;
  assert(cursorWheel().pulse == -1); // simultaneous rest/release
  input.activation = input.contact = true; input.atRest = false; input.y = .1f;
  assert(cursorWheel().selected == 0);
  input.activation = false; result = cursorWheel(); assert(result.pulse == 0 && !result.cursor);
  input.activation = true; assert(cursorWheel(VirtualMenuSelection::TOUCH_RELEASE).selected == 0);
  input.contact = false; input.atRest = true; input.y = .5f;
  result = cursorWheel(VirtualMenuSelection::TOUCH_RELEASE); assert(result.pulse == 0 && result.selected == -1);
  assert(cursorWheel(VirtualMenuSelection::TOUCH_RELEASE).pulse == -1);
  input.confirm = true; assert(cursorWheel(VirtualMenuSelection::CLICK).pulse == -1);
  assert(cursorWheel(VirtualMenuSelection::CONTINUOUS).held == -1);
  input.activation = false; assert(cursorWheel(VirtualMenuSelection::ACTIVATION_RELEASE, true).pulse == 25);
  VirtualMenuRouting cursorGrid;
  input = {}; input.activation = input.contact = input.joystickCursor = true; input.x = input.y = .75f;
  assert(cursorGrid.update(VirtualMenuType::TOUCH, VirtualMenuActivation::HOLD, VirtualMenuSelection::ACTIVATION_RELEASE, 4, 2, .2f, input).selected == 3);
  input.activation = input.contact = false; input.atRest = true; input.x = input.y = .5f;
  assert(cursorGrid.update(VirtualMenuType::TOUCH, VirtualMenuActivation::HOLD, VirtualMenuSelection::ACTIVATION_RELEASE, 4, 2, .2f, input).pulse == -1);
  VirtualMenuRouting visibility;
  input = {}; input.activation = true;
  auto visibleState = [&] { return visibility.update(VirtualMenuType::RADIAL, VirtualMenuActivation::HOLD, VirtualMenuSelection::CLICK, 8, 8, .2f, input); };
  assert(!visibleState().navigating);
  input.contact = true;
  assert(visibleState().navigating && visibleState().selected == -1); // centre contact
  input.y = .1f;
  assert(visibleState().navigating && visibleState().selected == 0);
  input.contact = false;
  assert(!visibleState().navigating && visibleState().selected == 0); // retained highlight
  input.digitalNavigation = input.contact = true;
  assert(!visibleState().navigating); // synthetic contact for continuous hotbars
  input.next = true; assert(visibleState().navigating);
  input.activation = false; assert(!visibleState().navigating);
  std::cout << "PASS: native menu preview, independent activation/navigation, release/click/contact/continuous selection, hotbar wrap/memory, joystick cursor neutral and same-poll cancellation, cancel and invalid values\n";
}
