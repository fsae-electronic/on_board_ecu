#include "Bridgetek_EVE2.hpp"
#include "dashboard.h"
#include "ui_touch.h"
#include "data.h"
#include "test_main_ecu.hpp"


#include "sys_core.h"

#include "rti.h"

extern "C" {
#include "ti_fee.h"
}

#define CLOCK_RTI_HZ 10000000 
#define SCREEN_UPDATE_TICKS (CLOCK_RTI_HZ / 30) // 30Hz update rate for the screen
#define DATA_UPDATE_TICKS (CLOCK_RTI_HZ / 60) // 60Hz update rate for data updates

Bridgetek_EVE2 display_data;


int main()
{

    _enable_interrupt_();

    display_data.setup(WQVGA);
    display_data.Init();

    rtiInit();
    TI_Fee_Init();
    while(TI_Fee_GetStatus(0) != IDLE)
    {
        TI_Fee_MainFunction();
    }

    
    //CLOCK RTI = 10Mhz
    // RTI Update rate for screen = 50ms (20Hz)
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
    //eve_calibrate(display_data);
    //saveCalibration(display_data);



    init_data();
    test_main_ecu_init();


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
        // This will be called at 60Hz, used for screen updates
        update_dashboard_draw(display_data, &dashboard_data);

    }
    else if (notification == rtiNOTIFICATION_COMPARE1)
    {
        update_dashboard_data(&dashboard_data);


        test_main_ecu_periodic();
        update_data();

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
            default:
                // Handle other message boxes if needed
                break;
        }
    }
}
 
