#include "ui_buttons.h"

void ui_draw_buttons(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    // DRIVE button
    if (d->drive_enabled) {
        eve.COLOR_RGB(0, 255, 0);  // Verde si ON
    } else {
        eve.COLOR_RGB(255, 0, 0);  // Rojo si OFF
    }
    eve.TAG(3);
    eve.CMD_BUTTON(
        20,230,130,30,26,0,
        d->drive_enabled ? "DRIVE ON":"DRIVE OFF"
    );

    // TRACTION button
    if (d->traction_on) {
        eve.COLOR_RGB(0, 255, 0);  // Verde si ON
    } else {
        eve.COLOR_RGB(255, 0, 0);  // Rojo si OFF
    }
    eve.TAG(1);
    eve.CMD_BUTTON(
        175,230,130,30,26,0,
        d->traction_on ? "TRACTION: ON":"TRACTION: OFF"
    );

    // MODE button
    if (d->mode == NORMAL) 
    {
        eve.COLOR_RGB(0, 255, 0);  // Verde si NORMAL
        eve.TAG(2);
        eve.CMD_BUTTON(
            330,230,130,30,26,0,
            "NORMAL"
        );
    } else {
        eve.COLOR_RGB(255, 0, 0);  // Rojo si RACE
        eve.TAG(2);
        eve.CMD_BUTTON(
            330,230,130,30,26,0,
            "RACE"
        );
    }

    eve.TAG(255);
}
