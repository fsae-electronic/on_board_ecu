#include "Bridgetek_EVE2.hpp"
#include "dashboard.h"
#include "ui_touch.h"
#include "data.h"
#include "inputs.h"
#include "test_main_ecu.hpp"


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

Bridgetek_EVE2 display_data;


int main()
{

    _enable_interrupt_();

    sciInit();
    sciSetBaudrate(sciREG, 115200U);

    // Print initialization message
    sciSend(sciREG, 64, (uint8_t*)("TMS570 ON-BOARD ECU INITIALIZED\n"));


    canInit();
    canEnableloopback(canREG1, Internal_Lbk); // Enable loopback mode for testing without actual CAN hardware

    sciSend(sciREG, 64, (uint8_t*)("CAN initialized\n"));

    sciSend(sciREG, 64, (uint8_t*)("Initializing display...\n"));
    display_data.setup(WQVGA);
    display_data.Init();
    sciSend(sciREG, 64, (uint8_t*)("Display initialized\n"));

    sciSend(sciREG, 64, (uint8_t*)("Initializing EEPROM...\n"));
    TI_Fee_Init();
    while(TI_Fee_GetStatus(0) != IDLE)
    {
        TI_Fee_MainFunction();
    }
    sciSend(sciREG, 64, (uint8_t*)("EEPROM initialized\n"));

    init_inputs();

    connect_input(INPUT_0, &dashboard_data.drive_enabled);
    connect_input(INPUT_1, &dashboard_data.traction_on);
    connect_input(INPUT_2, &dashboard_data.mode);
    connect_input(INPUT_3, &dashboard_data.telemetry_enabled);
    

    rtiInit();

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
    //eve_calibrate(display_data);
    //saveCalibration(display_data);



    //init_data();
    //test_main_ecu_init();


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


        //test_main_ecu_periodic();
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
            default:
                // Handle other message boxes if needed
                break;
        }
    }
}
 
