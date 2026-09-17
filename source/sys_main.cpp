#include "Bridgetek_EVE2.hpp"
#include "dashboard.h"
#include "ui_touch.h"
#include "data.h"
#include "inputs.h"
#include "logo.h"


#include "sys_core.h"

#include "rti.h"
#include "sci.h"
#include "can.h"

extern "C" {
#include "ti_fee.h"
}

#define CLOCK_RTI_HZ 10000000 
#define SCREEN_UPDATE_TICKS (CLOCK_RTI_HZ / 30) // 30Hz update rate for the screen
#define DATA_UPDATE_TICKS (CLOCK_RTI_HZ / 60) // 60Hz update rate for data updates
#define LOGO_SPLASH_SECONDS 3

Bridgetek_EVE2 display_data;

static void show_logo_splash(Bridgetek_EVE2 &eve)
{
    const uint32_t linestride = (LOGO_WIDTH + 7U) / 8U;
    const uint32_t frame_ticks = CLOCK_RTI_HZ / 30U;
    const uint32_t splash_ticks =
        (uint32_t)LOGO_SPLASH_SECONDS * CLOCK_RTI_HZ;

    eve.LIB_WriteDataToRAMG(
        epd_bitmap_itba_competicion,
        sizeof(epd_bitmap_itba_competicion),
        eve.RAM_G
    );

    rtiResetCounter(rtiCOUNTER_BLOCK0);
    rtiStartCounter(rtiCOUNTER_BLOCK0);

    uint32_t next_frame = 0;

    while(rtiREG1->CNT[0].FRCx < splash_ticks)
    {
        uint32_t elapsed = rtiREG1->CNT[0].FRCx;

        if(elapsed < next_frame)
        {
            continue;
        }

        next_frame += frame_ticks;

        float progress = (float)elapsed / (float)splash_ticks;

        if(progress > 1.0f)
        {
            progress = 1.0f;
        }

        // Ease-out: empieza rápido y termina suavemente.
        float inverse = 1.0f - progress;
        float eased = 1.0f - inverse * inverse;

        const float start_scale = 0.70f;
        float scale = start_scale +
                      (1.0f - start_scale) * eased;

        uint16_t width = (uint16_t)(LOGO_WIDTH * scale);
        uint16_t height = (uint16_t)(LOGO_HEIGHT * scale);

        int16_t x = (int16_t)((eve.DISP_WIDTH() - width) / 2);
        int16_t y = (int16_t)((eve.DISP_HEIGHT() - height) / 2);

        eve.LIB_BeginCoProList();
        eve.CMD_DLSTART();

        eve.CLEAR_COLOR_RGB(0, 0, 0);
        eve.CLEAR(1, 1, 1);

        eve.COLOR_RGB(255, 255, 255);
        eve.BITMAP_HANDLE(0);
        eve.BITMAP_SOURCE(eve.RAM_G);
        eve.BITMAP_LAYOUT(eve.FORMAT_L1, linestride, LOGO_HEIGHT);

        eve.BITMAP_SIZE(
            1,
            0,
            0,
            width,
            height
        );

        eve.BEGIN(eve.BEGIN_BITMAPS);
        eve.VERTEX2F(x * 16, y * 16);
        eve.END();

        eve.DISPLAY();
        eve.CMD_SWAP();

        eve.LIB_EndCoProList();
        eve.LIB_AwaitCoProEmpty();
    }

    rtiStopCounter(rtiCOUNTER_BLOCK0);
}


int main()
{

    _enable_interrupt_();

    sciInit();
    sciSetBaudrate(sciREG, 115200U);

    canInit();

    init_inputs();

    connect_input(INPUT_0, &dashboard_data.drive_enabled);
    connect_input(INPUT_1, &dashboard_data.traction_on);
    connect_input(INPUT_2, &dashboard_data.mode);
    connect_input(INPUT_3, &dashboard_data.telemetry_enabled);

    // Delay for a short period to allow peripherals to stabilize
    for (volatile int i = 0; i < 1000000; i++);


    display_data.setup(WQVGA);
    display_data.Init();

    rtiInit();

    show_logo_splash(display_data);

    TI_Fee_Init();
    while(TI_Fee_GetStatus(0) != IDLE)
    {
        TI_Fee_MainFunction();
    }


    


    //CLOCK RTI = 10Mhz
    rtiSetPeriod(rtiCOMPARE0, SCREEN_UPDATE_TICKS);
    rtiEnableNotification(rtiNOTIFICATION_COMPARE0);

    rtiSetPeriod(rtiCOMPARE1, DATA_UPDATE_TICKS);
    rtiEnableNotification(rtiNOTIFICATION_COMPARE1);


        // try restoring previous touch + debug calibration from EEPROM
    if (!loadCalibration(display_data)) {
        // if not found, perform fresh touch calibration
        eve_calibrate(display_data);
        saveCalibration(display_data);
    }




    rtiStartCounter(rtiCOUNTER_BLOCK0);              /* Start RTI counter block 0 */
    init_dashboard(&dashboard_data);


    while(1)
    {

    }
}

void rtiNotification(uint32 notification)
{
    if (notification == rtiNOTIFICATION_COMPARE0)
    {
        update_dashboard_draw(display_data, &dashboard_data);

    }
    else if (notification == rtiNOTIFICATION_COMPARE1)
    {
        update_dashboard_data(&dashboard_data);


        update_data();
        update_inputs();

        ui_handle_touch(display_data, &dashboard_data);

    }
}

void canMessageNotification(canBASE_t *node, uint32 messageBox)
{
    if(node == canREG1)
    {
        switch(messageBox)
        {
            case canMESSAGE_BOX1:
                tps_data.new_data = true;
                break;
            case canMESSAGE_BOX2:
                front_data.new_data = true;
                break;
            case canMESSAGE_BOX3:
                current_data.new_data = true;
                break;
            case canMESSAGE_BOX4:
                rear_data.new_data = true;
                break;
            case canMESSAGE_BOX5:
                driver1_status.new_data = true;
                break;
            case canMESSAGE_BOX6:
                driver2_status.new_data = true;
                break;
            case canMESSAGE_BOX7:
                motor1_data.new_data = true;
                break;
            case canMESSAGE_BOX8:
                motor2_data.new_data = true;
                break;
            case canMESSAGE_BOX9:
                driver1_data.new_data = true;
                break;
            case canMESSAGE_BOX10:
                driver2_data.new_data = true;
                break;
            case canMESSAGE_BOX11:
                main_ecu_data.new_data = true;
                break;
            case canMESSAGE_BOX12:
                canopen_heartbeat.new_data = true;
                break;
            default:
                // Handle other message boxes if needed
                break;
        }
    }
}
 
