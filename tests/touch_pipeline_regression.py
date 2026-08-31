from pathlib import Path

ROOT = Path(__file__).parents[1]
HEADER = (ROOT / 'JoyShockMapper/include/JoyShock.h').read_text()
MAIN = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
APP = (ROOT.parent / 'JSM_GUI/jsm_gui_tauri/src/App.tsx').read_text()


def test_prediction_starts_without_origin_overshoot_and_is_bounded():
    body = HEADER.split('struct TouchMousePipeline', 1)[1].split('};', 1)[0]
    assert 'if (!initialized)' in body
    assert 'filteredX = x; filteredY = y;' in body
    assert 'dx < -2.f' in body and 'dx > 2.f' in body
    assert 'dy < -2.f' in body and 'dy > 2.f' in body
    assert 'filteredX = a * x + (1.f - a) * filteredX;' in body
    assert 'filteredY = a * y + (1.f - a) * filteredY;' in body


def test_contact_threshold_gates_sdl_down_and_pressure_fallback():
    sdl = (ROOT / 'JoyShockMapper/src/SDLWrapper.cpp').read_text()
    assert 'state.t0Down = pressure0 >= threshold;' in sdl
    assert 'state.t1Down = pressure1 >= threshold;' in sdl
    assert 'state.t0Down = pressure0 > 0.001f;' in sdl
    assert 'state.t1Down = pressure1 > 0.001f;' in sdl
    # SDL's t0Down/t1Down is no longer required for pressure promotion
    assert 'state.t0Down = state.t0Down && pressure0 >= threshold;' not in sdl
    assert 'state.t1Down = state.t1Down && pressure1 >= threshold;' not in sdl


def test_pipeline_retains_acceleration_and_trackball_friction():
    assert 'touchPipelines[2]' in HEADER
    assert 'TRACKBALL_DECAY' in MAIN
    assert 'TOUCHPAD_ACCELERATION' in MAIN
    assert 'momentumX *= decay' in MAIN
    assert 'momentumY *= decay' in MAIN


def test_each_touchpad_uses_own_pipeline_and_resets_on_release():
    assert 'js->touchPipelines[0].reset();' in MAIN
    assert 'js->touchPipelines[1].reset();' in MAIN
    assert 'js->touchMomentumX' not in MAIN
    assert 'js->touchActive' not in MAIN


def test_keymap_call_sites_forward_light_touch_threshold():
    assert APP.count('lightTouchThreshold={lightTouchThreshold}') >= 2
    assert APP.count('onLightTouchThresholdChange={handleLightTouchThresholdChange}') >= 2


if __name__ == '__main__':
    for name, test in sorted(globals().items()):
        if name.startswith('test_'):
            test()
            print(f'PASS {name}')
