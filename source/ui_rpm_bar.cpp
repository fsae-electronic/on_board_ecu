#include "ui_rpm_bar.h"

#define MAX_RPM 5000
#define LED_COUNT 20

void ui_draw_rpm_bar(Bridgetek_EVE2 &eve, float rpm)
{
    int leds_on = (rpm / MAX_RPM) * LED_COUNT;

    for(int i=0;i<LED_COUNT;i++)
    {
        int x = 40 + i*20;
        int y = 60;

        if(i < leds_on)
        {
            if(i < 12)
                eve.COLOR_RGB(255,255,0);
            else if(i < 17)
                eve.COLOR_RGB(255,120,0);
            else
                eve.COLOR_RGB(255,0,0);
        }
        else
            eve.COLOR_RGB(40,40,40);

        eve.BEGIN(Bridgetek_EVE2::BEGIN_RECTS);
        eve.VERTEX2F(x*16,y*16);
        eve.VERTEX2F((x+15)*16,(y+10)*16);
        eve.END();
    }

    eve.COLOR_RGB(255,255,255);

    eve.CMD_NUMBER(
        80,
        30,
        31,
        Bridgetek_EVE2::OPT_CENTER,
        rpm
    );
    eve.CMD_TEXT(
        160,
        40,
        26,
        Bridgetek_EVE2::OPT_CENTER,
        "RPM"
    );
}
