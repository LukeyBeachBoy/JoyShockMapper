#include "touch_mouse_test_support.h"

static void tick(shared_ptr<JoyShock>& js, float x, float pressure) {
  TOUCH_POINT point{x, x < 0 ? -1.f : .5f};
  processTouchMouse(js, 1, point, 1, {1920,1920}, {1,1}, .003f, pressure);
}
int main() {
  bool down = false;
  for (float p : {.02f, .05f, .079f, .07f}) {
    down = digitalTriggerPressed(down, p, .08f, .02f);
    assert(!down);
  }
  down = digitalTriggerPressed(down, .081f, .08f, .02f);
  assert(down);
  for (int i=0; i<100; ++i) {
    down = digitalTriggerPressed(down, i%2 ? .079f : .081f, .08f, .02f);
    assert(down);
  }
  assert(!digitalTriggerPressed(down, .059f, .08f, .02f));
  assert(!digitalTriggerPressed(true, .079f, .08f, 0));
  assert(!digitalTriggerPressed(true, 0, .01f, .1f));

  TouchLiftGuard guard;
  assert(guard.update(.01f, 0, 150) == 1);
  assert(guard.update(.0095f, 20, 150) == 1); // pressure noise
  assert(guard.update(.008f, 20, 150) < 1);
  assert(guard.update(.006f, 20, 150) == 0);
  assert(guard.update(.01f, 20, 150) == 1); // repress recovers
  assert(guard.update(.001f, 1000, 150) == 1); // flick stays responsive
  guard.reset();
  assert(guard.update(0, 20, 150) == 1); // no force channel
  assert(guard.update(.001f, 20, 0) == 1); // disabled

  auto js=std::make_shared<JoyShock>();
  js->settings[SettingID::TOUCHPAD_MIN_CUTOFF]=0;
  js->settings[SettingID::TOUCHPAD_TRACKBALL_DECAY]=20;
  tick(js,.5f,.02f);
  for(int i=0;i<20;++i) tick(js,.5f,.02f); // stationary thumb
  const auto start=totalX;
  for(int i=1;i<=15;++i) tick(js,.5f+i*.0001f,.012f-i*.0005f);
  tick(js,-1,0);
  for(int i=0;i<100;++i) tick(js,-1,0);
  assert(std::abs(totalX-start)<.001); // no spring, queued tail or coast
  tick(js,.8f,.02f);
  const auto before=totalX;
  tick(js,.81f,.001f); // fast release flick unaffected
  tick(js,-1,0);
  assert(totalX>before+10);
  puts("PASS: digital hysteresis, pressure noise, stationary lift, no release debt, re-touch and flick escape");
}
