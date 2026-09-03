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
# The patch ORs the capacitive bit in rather than replacing the pressure test, so
# it can only ever add contacts, never remove one. If a firmware revision stops
# setting those bits, the pads still work exactly as they do today.
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
        set(_replacement
            "((pTritonReport->buttons & TRITON_${_SIDE}_TOUCHPAD_TOUCH) != 0 || pTritonReport->sPressure${_side} > 0),")

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

    if(NOT _patched)
        file(WRITE "${_driver}" "${_source}")
        message(STATUS "Patched SDL: Steam Controller pads report capacitive contact")
    endif()
endfunction()
