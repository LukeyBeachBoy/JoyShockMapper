# Make the Steam Controller 2026 pads report contact from their capacitive sense
# instead of from pad pressure.
#
# SDL's Triton driver builds the touchpad finger-down flag out of the pressure
# channel:
#
#     SDL_SendJoystickTouchpad(..., pTritonReport->sPressureLeft > 0, ...)
#
# while TRITON_LEFT_TOUCHPAD_TOUCH / TRITON_RIGHT_TOUCHPAD_TOUCH -- the pads' own
# capacitive contact bits, already decoded in the same report -- go unused. The
# result is that resting a finger on the pad reports nothing: you have to press
# hard enough to register force before the pad reports a touch at all, and the
# pressure reading hovers in the noise right at the boundary, so contact flickers.
# That friction is exactly what makes a slow, deliberate swipe difficult.
#
# Contact must end when the firmware touch bit clears, even if pressure still
# contains residual force/noise. OR-ing pressure into contact caused phantom
# touches and cursor movement after release. This follows upstream SDL's
# capacitive-only contact decision. Release positions are supplied as zero so
# SDL_SendJoystickTouchpad preserves the last valid coordinates.
#
# Done as a source rewrite because SDL is fetched by CPM 0.27.1, which predates
# its PATCHES argument, and a .patch file would need a patch binary on the build
# machine. Both directions are checked, so an SDL bump that changes this code
# fails the configure step loudly instead of silently dropping the fix.

function(patch_sdl_triton_touch SDL_SOURCE_DIR)
    set(_driver "${SDL_SOURCE_DIR}/src/joystick/hidapi/SDL_hidapi_steam_triton.c")
    if(NOT EXISTS "${_driver}")
        message(FATAL_ERROR
            "Steam Controller 2026 touch patch: ${_driver} does not exist. "
            "Did the SDL layout change?")
    endif()

    file(READ "${_driver}" _source)

    set(_patched TRUE)
    foreach(_side Left Right)
        string(TOUPPER "${_side}" _SIDE)
        set(_original "pTritonReport->sPressure${_side} > 0,")
        set(_legacy "((pTritonReport->buttons & TRITON_${_SIDE}_TOUCHPAD_TOUCH) != 0 || pTritonReport->sPressure${_side} > 0),")
        set(_replacement
            "((pTritonReport->buttons & TRITON_${_SIDE}_TOUCHPAD_TOUCH) != 0),")
        string(FIND "${_source}" "${_legacy}" _legacy_found)
        if(_legacy_found GREATER_EQUAL 0)
            string(REPLACE "${_legacy}" "${_replacement}" _source "${_source}")
            set(_patched FALSE)
        endif()

        # SDL preserves its previous coordinates when a release sends (0, 0).
        # Never publish the no-contact coordinate fields as a new position.
        set(_x "pTritonReport->s${_side}PadX / 65536.0f + 0.5f,")
        set(_y "-(float)pTritonReport->s${_side}PadY / 65536.0f + 0.5f,")
        set(_x_release "((pTritonReport->buttons & TRITON_${_SIDE}_TOUCHPAD_TOUCH) ? pTritonReport->s${_side}PadX / 65536.0f + 0.5f : 0.0f),")
        set(_y_release "((pTritonReport->buttons & TRITON_${_SIDE}_TOUCHPAD_TOUCH) ? -(float)pTritonReport->s${_side}PadY / 65536.0f + 0.5f : 0.0f),")
        string(FIND "${_source}" "${_x_release}" _coords_already)
        if(_coords_already LESS 0)
            string(FIND "${_source}" "${_x}" _x_found)
            string(FIND "${_source}" "${_y}" _y_found)
            if(_x_found LESS 0 OR _y_found LESS 0)
                message(FATAL_ERROR "Steam Controller touch patch: could not find ${_side} coordinates")
            endif()
            string(REPLACE "${_x}" "${_x_release}" _source "${_source}")
            string(REPLACE "${_y}" "${_y_release}" _source "${_source}")
            set(_patched FALSE)
        endif()

        string(FIND "${_source}" "${_replacement}" _already)
        if(_already GREATER_EQUAL 0)
            continue()
        endif()

        string(FIND "${_source}" "${_original}" _found)
        if(_found LESS 0)
            message(FATAL_ERROR
                "Steam Controller 2026 touch patch: could not find the ${_side} pad's "
                "finger-down expression in SDL_hidapi_steam_triton.c. Re-check the "
                "driver against the pinned SDL commit before bumping it.")
        endif()

        string(REPLACE "${_original}" "${_replacement}" _source "${_source}")
        set(_patched FALSE)
    endforeach()

    # --- Let a haptic pulse through SendJoystickEffect ------------------------
    #
    # The grips have their own haptic actuators, driven by OUTPUT reports rather
    # than the feature report SendJoystickEffect accepts, so there is no way to
    # reach them from outside SDL at all. The driver already writes output reports
    # for rumble; this accepts the haptic ones by their own report id -- the
    # 0x80..0x85 block the driver's own header defines -- and writes them the same
    # way. Anything else still returns SDL_Unsupported exactly as before.
    set(_effect_original "    if (size == HID_FEATURE_REPORT_BYTES) {")
    set(_effect_replacement "    const Uint8 *jsm_bytes = (const Uint8 *)data;\n    if (size > 1 && size < HID_FEATURE_REPORT_BYTES &&\n        jsm_bytes[0] >= ID_OUT_REPORT_HAPTIC_RUMBLE && jsm_bytes[0] <= ID_OUT_REPORT_HAPTIC_SCRIPT) {\n        return SDL_hid_write(device->dev, data, size) == size;\n    }\n    if (size == HID_FEATURE_REPORT_BYTES) {")

    string(FIND "${_source}" "jsm_bytes" _haptic_already)
    if(_haptic_already LESS 0)
        string(FIND "${_source}" "${_effect_original}" _found)
        if(_found LESS 0)
            message(FATAL_ERROR
                "Steam Controller haptic patch: could not find SendJoystickEffect's "
                "feature-report branch in SDL_hidapi_steam_triton.c.")
        endif()
        string(REPLACE "${_effect_original}" "${_effect_replacement}" _source "${_source}")
        set(_patched FALSE)
    endif()

    if(NOT _patched)
        file(WRITE "${_driver}" "${_source}")
        message(STATUS "Patched SDL: capacitive pad contact + grip haptic pulses")
    endif()
endfunction()
