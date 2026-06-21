#include "data.h"
#include "dashboard.h"
#include "sci.h"

#include "can.h"
#include <string.h>


tps_data_t tps_data;
front_data_t front_data;

current_data_t current_data;
rear_data_t rear_data;

driver_status_t driver1_status(DRIVER1_STATUS_ID);
driver_status_t driver2_status(DRIVER2_STATUS_ID);

motor_data_t motor1_data(MOTOR1_DATA_ID);
motor_data_t motor2_data(MOTOR2_DATA_ID);

driver_data_t driver1_data(DRIVER1_DATA_ID);
driver_data_t driver2_data(DRIVER2_DATA_ID);

main_ecu_data_t main_ecu_data;

buttons_data_t buttons_data;
canopen_heartbeat_t canopen_heartbeat;
calibration_cmd_t calibration_cmd;



void init_data(void)
{
    canInit();
    canEnableloopback(canREG1, Internal_Lbk); // Enable loopback mode for testing without actual CAN hardware

}


void update_data(void)
{
    static uint8_t last_cal_tps_0 = 0;
    static uint8_t last_cal_tps_100 = 0;
    static uint8_t last_cal_left_steer = 0;
    static uint8_t last_cal_center_steer = 0;
    static uint8_t last_cal_right_steer = 0;
    static uint8_t last_cal_current_sensors = 0;

    auto send_calibration_cmd = [](uint8_t cmd_id)
    {
        calibration_cmd.values.cmd_id = cmd_id;
        canTransmit(canREG1, canMESSAGE_BOX18, calibration_cmd.raw);
    };

    // This function can be used to perform any periodic updates or checks for the drivers if needed
    // Send data of buttons status to dashboard
    if(tps_data.new_data) 
    {
        canGetData(canREG1, canMESSAGE_BOX1, tps_data.raw);
        dashboard_data.tps_1 = (float)tps_data.values.tps_1;
        dashboard_data.tps_2 = (float)tps_data.values.tps_2;
        dashboard_data.tps = (dashboard_data.tps_1 + dashboard_data.tps_2) / 2.0f; // Average of both TPS sensors
        tps_data.new_data = false;
    }
    if(front_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX2, front_data.raw);
        dashboard_data.wheel_speed_fl = (float)front_data.values.front_left_speed;
        dashboard_data.wheel_speed_fr = (float)front_data.values.front_right_speed;
        dashboard_data.steering_angle = (float)front_data.values.direction; // Assuming direction is in degrees
        dashboard_data.brake_front = (float)front_data.values.front_brake_pressure;
        front_data.new_data = false;
    }
    if(current_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX3, current_data.raw);
        dashboard_data.motor1_ac_current = (float)current_data.values.ac_current_1;
        dashboard_data.driver1_dc_current = (float)current_data.values.dc_current_1;
        dashboard_data.motor2_ac_current = (float)current_data.values.ac_current_2;
        dashboard_data.driver2_dc_current = (float)current_data.values.dc_current_2;
        current_data.new_data = false;
    }
    if(rear_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX4, rear_data.raw);
        dashboard_data.brake_rear = (float)rear_data.values.rear_brake_pressure;
        rear_data.new_data = false;
    }
    if(driver1_status.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX5, driver1_status.raw);
        dashboard_data.driver1_warning = driver1_status.values.warning;
        dashboard_data.driver1_error = driver1_status.values.error;
        driver1_status.new_data = false;
    }
    if(driver2_status.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX6, driver2_status.raw);
        dashboard_data.driver2_warning = driver2_status.values.warning;
        dashboard_data.driver2_error = driver2_status.values.error;
        driver2_status.new_data = false;
    }
    if(motor1_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX7, motor1_data.raw);
        dashboard_data.wheel_speed_rl = (float)motor1_data.values.motor_velocity; // Assuming motor velocity can be used to calculate rear wheel speed
        dashboard_data.motor1_rated_current = (float)motor1_data.values.motor_rated_current;
        dashboard_data.motor1_temp = (float)motor1_data.values.motor_temp;
        motor1_data.new_data = false;
    }
    if(motor2_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX8, motor2_data.raw);
        dashboard_data.wheel_speed_rr = (float)motor2_data.values.motor_velocity; // Assuming motor velocity can be used to calculate rear wheel speed
        dashboard_data.motor2_rated_current = (float)motor2_data.values.motor_rated_current;
        dashboard_data.motor2_temp = (float)motor2_data.values.motor_temp;
        motor2_data.new_data = false;
    }
    if(driver1_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX9, driver1_data.raw);
        dashboard_data.driver1_temp = (float)driver1_data.values.driver_temp;
        dashboard_data.driver1_dc_voltage = (float)driver1_data.values.driver_dc_voltage;
        driver1_data.new_data = false;
    }
    if(driver2_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX10, driver2_data.raw);
        dashboard_data.driver2_temp = (float)driver2_data.values.driver_temp;
        dashboard_data.driver2_dc_voltage = (float)driver2_data.values.driver_dc_voltage;
        driver2_data.new_data = false;
    }
    if(main_ecu_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX11, main_ecu_data.raw);
        main_ecu_data.values.tps = dashboard_data.main_ecu_tps; // Use the averaged TPS value from the dashboard
        main_ecu_data.values.mode = dashboard_data.main_ecu_mode;
        main_ecu_data.values.error_code = dashboard_data.main_ecu_error_code; // No error for now, can
        main_ecu_data.new_data = false;
    }
    if(canopen_heartbeat.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX12, canopen_heartbeat.raw);
        dashboard_data.canopen_state = canopen_heartbeat.values.canopen_state;
        canopen_heartbeat.new_data = false;
    }
    // Process data
    dashboard_data.rpm = (dashboard_data.wheel_speed_rl + dashboard_data.wheel_speed_rr) / 2.0f;
    dashboard_data.battery_voltage = (dashboard_data.driver1_dc_voltage + dashboard_data.driver2_dc_voltage) / 2.0f;
    dashboard_data.battery_current = (dashboard_data.driver1_dc_current + dashboard_data.driver2_dc_current);
    


    // Send calibration commands only on rising edge and only in PRE_OPERATIONAL.
    if(dashboard_data.canopen_state == PRE_OPERATIONAL)
    {
        if(dashboard_data.cal_tps_0 && !last_cal_tps_0)
        {
            send_calibration_cmd(CAL_CMD_TPS_0);
        }
        if(dashboard_data.cal_tps_100 && !last_cal_tps_100)
        {
            send_calibration_cmd(CAL_CMD_TPS_100);
        }
        if(dashboard_data.cal_left_steer && !last_cal_left_steer)
        {
            send_calibration_cmd(CAL_CMD_LEFT_STEER);
        }
        if(dashboard_data.cal_center_steer && !last_cal_center_steer)
        {
            send_calibration_cmd(CAL_CMD_CENTER_STEER);
        }
        if(dashboard_data.cal_right_steer && !last_cal_right_steer)
        {
            send_calibration_cmd(CAL_CMD_RIGHT_STEER);
        }
        if(dashboard_data.cal_current_sensors && !last_cal_current_sensors)
        {
            send_calibration_cmd(CAL_CMD_CURRENT_SENSORS);
        }
    }

    last_cal_tps_0 = dashboard_data.cal_tps_0;
    last_cal_tps_100 = dashboard_data.cal_tps_100;
    last_cal_left_steer = dashboard_data.cal_left_steer;
    last_cal_center_steer = dashboard_data.cal_center_steer;
    last_cal_right_steer = dashboard_data.cal_right_steer;
    last_cal_current_sensors = dashboard_data.cal_current_sensors;


}

// CAN message notification callback
// This function will be called by the CAN driver when a new message is 
// received in one of the configured message boxes




