#include "data.h"
#include "dashboard.h"
#include "test_data.h"
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

implausibility_data_t implausibility_data;
main_ecu_errors_t main_ecu_errors;

buttons_data_t buttons_data;
canopen_heartbeat_t canopen_heartbeat;
calibration_cmd_t calibration_cmd;

charger_cmd_t charger_cmd;
charger_data_t charger_data;

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
    static uint8_t last_drive_enabled = 0xFF;
    static uint8_t last_traction_on = 0xFF;
    static uint8_t last_mode = 0xFF;
    static uint8_t last_telemetry_enabled = 0xFF;

    if(test_data_is_enabled())
    {
        test_data_update(&dashboard_data);
        return; // In test mode do not send any CAN message.
    }

    auto send_calibration_cmd = [](uint8_t cmd_id)
    {
        calibration_cmd.values.cmd_id = cmd_id;
        calibration_cmd.values.value = 0;
        canTransmit(canREG1, canMESSAGE_BOX18, calibration_cmd.raw);
    };

    // This function can be used to perform any periodic updates or checks for the drivers if needed
    // Send data of buttons status to dashboard
    if (tps_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX1, tps_data.raw);

        uint16_t tps_1 = (uint16_t)tps_data.raw[0] | ((uint16_t)tps_data.raw[1] << 8);
        uint16_t tps_2 = (uint16_t)tps_data.raw[2] | ((uint16_t)tps_data.raw[3] << 8);

        dashboard_data.tps_1 = (float)tps_1;
        dashboard_data.tps_2 = (float)tps_2;
        dashboard_data.tps = (dashboard_data.tps_1 + dashboard_data.tps_2) / 2.0f; // Average of both TPS sensors
        tps_data.new_data = false;
    }
    if (front_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX2, front_data.raw);
        // Reconstruct 16-bit values from raw bytes explicitly (little-endian: LSB first)
        uint16_t fl_speed = (uint16_t)front_data.raw[0] | ((uint16_t)front_data.raw[1] << 8);
        uint16_t fr_speed = (uint16_t)front_data.raw[2] | ((uint16_t)front_data.raw[3] << 8);
        int16_t direction = (int16_t)((uint16_t)front_data.raw[4] | ((uint16_t)front_data.raw[5] << 8)); // signed: negative = left of center
        uint16_t front_brake = (uint16_t)front_data.raw[6] | ((uint16_t)front_data.raw[7] << 8);

        dashboard_data.wheel_speed_fl = (float)fl_speed;
        dashboard_data.wheel_speed_fr = (float)fr_speed;
        dashboard_data.steering_angle = (float)direction; // Assuming direction is in degrees
        dashboard_data.brake_front = (float)front_brake;
        front_data.new_data = false;
    }
    if (current_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX3, current_data.raw);
        // Reconstruct 16-bit currents (little-endian LSB first)
        uint16_t ac1 = (uint16_t)current_data.raw[0] | ((uint16_t)current_data.raw[1] << 8);
        uint16_t ac2 = (uint16_t)current_data.raw[2] | ((uint16_t)current_data.raw[3] << 8);
        uint16_t dc1 = (uint16_t)current_data.raw[4] | ((uint16_t)current_data.raw[5] << 8);
        uint16_t dc2 = (uint16_t)current_data.raw[6] | ((uint16_t)current_data.raw[7] << 8);

        dashboard_data.motor1_ac_current = (float)ac1;
        dashboard_data.driver1_dc_current = (float)dc1;
        dashboard_data.motor2_ac_current = (float)ac2;
        dashboard_data.driver2_dc_current = (float)dc2;
        current_data.new_data = false;
    }
    if (rear_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX4, rear_data.raw);
        uint16_t rear_brake = (uint16_t)rear_data.raw[0] | ((uint16_t)rear_data.raw[1] << 8);
        dashboard_data.brake_rear = (float)rear_brake;
        rear_data.new_data = false;
    }
    if (driver1_status.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX5, driver1_status.raw);
        uint16_t warn1 = (uint16_t)driver1_status.raw[2] | ((uint16_t)driver1_status.raw[3] << 8);
        uint16_t err1  = (uint16_t)driver1_status.raw[4] | ((uint16_t)driver1_status.raw[5] << 8);
        dashboard_data.driver1_warning = (warn1 == 0U) ? NO_WARNING : (uint8_t)warn1;
        dashboard_data.driver1_error = err1;
        driver1_status.new_data = false;
    }
    if (driver2_status.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX6, driver2_status.raw);
        uint16_t warn2 = (uint16_t)driver2_status.raw[2] | ((uint16_t)driver2_status.raw[3] << 8);
        uint16_t err2  = (uint16_t)driver2_status.raw[4] | ((uint16_t)driver2_status.raw[5] << 8);
        dashboard_data.driver2_warning = (warn2 == 0U) ? NO_WARNING : (uint8_t)warn2;
        dashboard_data.driver2_error = err2;
        driver2_status.new_data = false;
    }
    if (motor1_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX7, motor1_data.raw);
        int32_t mv1 = (int32_t)motor1_data.raw[0] | ((int32_t)motor1_data.raw[1] << 8) | ((int32_t)motor1_data.raw[2] << 16) | ((int32_t)motor1_data.raw[3] << 24);
        uint16_t d1volt = (uint16_t)motor1_data.raw[4] | ((uint16_t)motor1_data.raw[5] << 8);

        dashboard_data.wheel_speed_rl = (-1.0f)*(float)mv1; // Assuming motor velocity can be used to calculate rear wheel speed
        dashboard_data.driver1_dc_voltage = (float)d1volt/10.0f;
        motor1_data.new_data = false;
    }
    if (motor2_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX8, motor2_data.raw);
        int32_t mv2 = (int32_t)motor2_data.raw[0] | ((int32_t)motor2_data.raw[1] << 8) | ((int32_t)motor2_data.raw[2] << 16) | ((int32_t)motor2_data.raw[3] << 24);
        uint16_t d2volt = (uint16_t)motor2_data.raw[4] | ((uint16_t)motor2_data.raw[5] << 8);

        dashboard_data.wheel_speed_rr = (-1.0f)*(float)mv2; // Assuming motor velocity can be used to calculate rear wheel speed
        dashboard_data.driver2_dc_voltage = (float)d2volt/10.0f;
        motor2_data.new_data = false;
    }
    if (driver1_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX9, driver1_data.raw);
        uint32_t mrc1 = (uint32_t)driver1_data.raw[0] | ((uint32_t)driver1_data.raw[1] << 8) | ((uint32_t)driver1_data.raw[2] << 16) | ((uint32_t)driver1_data.raw[3] << 24);
        uint8_t d1temp = driver1_data.raw[4];
        uint8_t m1temp = driver1_data.raw[5];
        dashboard_data.motor1_rated_current = (float)mrc1;
        dashboard_data.driver1_temp = (float)d1temp;
        dashboard_data.motor1_temp = (float)m1temp;
        driver1_data.new_data = false;
    }
    if (driver2_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX10, driver2_data.raw);
        uint32_t mrc2 = (uint32_t)driver2_data.raw[0] | ((uint32_t)driver2_data.raw[1] << 8) | ((uint32_t)driver2_data.raw[2] << 16) | ((uint32_t)driver2_data.raw[3] << 24);
        uint8_t d2temp = driver2_data.raw[4];
        uint8_t m2temp = driver2_data.raw[5];
        dashboard_data.motor2_rated_current = (float)mrc2;
        dashboard_data.driver2_temp = (float)d2temp;
        dashboard_data.motor2_temp = (float)m2temp;
        driver2_data.new_data = false;
    }
    if (implausibility_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX11, implausibility_data.raw);
        uint8_t flags = 0;
        if (implausibility_data.values.tps_implausibility != 0U)   flags |= IMPLAUSIBILITY_TPS;
        if (implausibility_data.values.brake_implausibility != 0U) flags |= IMPLAUSIBILITY_BRAKE;

        if (flags != 0U)
        {
            // The main ECU already forced drive off. Mirror it locally WITHOUT sending a 0x600:
            // updating last_drive_enabled too keeps the change detection below silent, so the
            // next press of the drive button sends CAL_CMD_DRIVE_ENABLE_ON directly.
            dashboard_data.drive_enabled = 0;
            last_drive_enabled = 0;
            dashboard_implausibility_raise(flags);
        }
        implausibility_data.new_data = false;
    }

    // EV.4.7 APPS/brake plausibility (0x403). The main ECU resumes on its own once the throttle
    // drops below 5 %, so this never touches drive_enabled nor sends any 0x600.
    static uint16_t main_ecu_errors_timeout = 0;
    auto set_apps_brake_plausibility = [](uint8_t active)
    {
        if (active && !dashboard_data.apps_brake_plausibility)
        {
            dashboard_log_event(0, LOG_FAULT, "APPS/brake plausibility: motors stopped");
        }
        else if (!active && dashboard_data.apps_brake_plausibility)
        {
            dashboard_log_event(0, LOG_INFO, "APPS/brake plausibility released");
        }
        dashboard_data.apps_brake_plausibility = active;
    };
    // Sensor CAN timeouts (0x403 b1). The main ECU forces drive off while any bit is set and
    // needs a new CAL_CMD_DRIVE_ENABLE_ON once they clear.
    auto set_sensor_can_timeout = [](uint8_t flags)
    {
        uint8_t added = (uint8_t)(flags & (uint8_t)~dashboard_data.sensor_can_timeout);
        if (added & SENSOR_TIMEOUT_TPS)         dashboard_log_event(0, LOG_FAULT, "CAN timeout: TPS (0x501)");
        if (added & SENSOR_TIMEOUT_FRONT_BRAKE) dashboard_log_event(0, LOG_FAULT, "CAN timeout: front brake (0x502)");
        if (added & SENSOR_TIMEOUT_REAR_BRAKE)  dashboard_log_event(0, LOG_FAULT, "CAN timeout: rear brake (0x504)");
        if ((flags == 0U) && (dashboard_data.sensor_can_timeout != 0U))
        {
            dashboard_log_event(0, LOG_INFO, "Sensor CAN timeout released");
        }
        dashboard_data.sensor_can_timeout = flags;
    };
    if (main_ecu_errors.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX16, main_ecu_errors.raw);
        set_apps_brake_plausibility((main_ecu_errors.values.apps_brake_plausibility == 1U) ? 1U : 0U);
        set_sensor_can_timeout((uint8_t)(main_ecu_errors.values.sensor_can_timeout & SENSOR_TIMEOUT_MASK));
        main_ecu_errors_timeout = 0;
        main_ecu_errors.new_data = false;
    }
    else if (dashboard_data.apps_brake_plausibility || dashboard_data.sensor_can_timeout)
    {
        // 0x403 is repeated every 100 ms while active: if it stops arriving, assume it was released.
        if (++main_ecu_errors_timeout >= MAIN_ECU_ERRORS_TIMEOUT_TICKS)
        {
            set_apps_brake_plausibility(0);
            set_sensor_can_timeout(0);
            main_ecu_errors_timeout = 0;
        }
    }
    if (dashboard_data.sensor_can_timeout)
    {
        // The main ECU forced drive off and rejects it while the timeout lasts. Mirror it locally
        // WITHOUT sending a 0x600 (last_drive_enabled = 0 keeps the change detection below silent),
        // also rejecting any request to enable drive from the button or the touch screen.
        dashboard_data.drive_enabled = 0;
        last_drive_enabled = 0;
    }
    if (canopen_heartbeat.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX12, canopen_heartbeat.raw);
        dashboard_data.canopen_state = canopen_heartbeat.values.canopen_state;
        canopen_heartbeat.new_data = false;
    }

    static uint16_t charger_timeout = 0;
    if (charger_cmd.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX14, charger_cmd.raw);
        dashboard_data.charging = (charger_cmd.raw[0] == 1U) ? 1U : 0U;
        dashboard_data.charger_flags = charger_cmd.raw[1];
        charger_timeout = 0;
        charger_cmd.new_data = false;
    }
    else if (dashboard_data.charging)
    {
        // 0x610 comes every 500 ms while charging: if it stops arriving, charging ended.
        if (++charger_timeout >= CHARGER_TIMEOUT_TICKS)
        {
            dashboard_data.charging = 0;
            charger_timeout = 0;
        }
    }
    if (charger_data.new_data)
    {
        canGetData(canREG1, canMESSAGE_BOX15, charger_data.raw);
        uint16_t cmd_v  = (uint16_t)charger_data.raw[0] | ((uint16_t)charger_data.raw[1] << 8);
        uint16_t cmd_i  = (uint16_t)charger_data.raw[2] | ((uint16_t)charger_data.raw[3] << 8);
        uint16_t meas_v = (uint16_t)charger_data.raw[4] | ((uint16_t)charger_data.raw[5] << 8);
        uint16_t meas_i = (uint16_t)charger_data.raw[6] | ((uint16_t)charger_data.raw[7] << 8);

        dashboard_data.charge_cmd_voltage = (float)cmd_v / 10.0f;
        dashboard_data.charge_cmd_current = (float)cmd_i / 10.0f;
        dashboard_data.charge_voltage = (float)meas_v / 10.0f;
        dashboard_data.charge_current = (float)meas_i / 400.0f;
        charger_data.new_data = false;
    }
    // Process data
    dashboard_data.rpm = (dashboard_data.wheel_speed_rl + dashboard_data.wheel_speed_rr) / 2.0f;
    dashboard_data.battery_voltage = (dashboard_data.driver1_dc_voltage + dashboard_data.driver2_dc_voltage) / 2.0f;
    dashboard_data.battery_current = (dashboard_data.driver1_dc_current + dashboard_data.driver2_dc_current);

    // Safety: drive can only be enabled with the front brake pressed. Any OFF -> ON request
    // (physical input or touch) without enough brake pressure is rejected before it is sent.
    if (dashboard_data.drive_enabled && (last_drive_enabled != 1U) &&
        !(dashboard_data.brake_front > DRIVE_ENABLE_MIN_BRAKE_PSI))
    {
        dashboard_data.drive_enabled = 0;
        dashboard_brake_alert_raise();
    }

    buttons_data.values.drive_enabled = (uint8_t)dashboard_data.drive_enabled;
    buttons_data.values.traction_on = (uint8_t)dashboard_data.traction_on;
    buttons_data.values.mode = (uint8_t)dashboard_data.mode;
    buttons_data.values.telemetry_enabled = (uint8_t)dashboard_data.telemetry_enabled;
    buttons_data.raw[4] = 0;
    buttons_data.raw[5] = 0;
    buttons_data.raw[6] = 0;
    buttons_data.raw[7] = 0;

    if ((buttons_data.values.drive_enabled != last_drive_enabled) ||
        (buttons_data.values.traction_on != last_traction_on) ||
        (buttons_data.values.mode != last_mode) ||
        (buttons_data.values.telemetry_enabled != last_telemetry_enabled))
    {
        canTransmit(canREG1, canMESSAGE_BOX17, buttons_data.raw);
    }

    // Send individual 0x600 calibration-style messages (byte[0] = cmd_id, byte[1] = 0)
    // on button state changes, one dedicated cmd_id per action.
    if (buttons_data.values.drive_enabled != last_drive_enabled)
    {
        if (buttons_data.values.drive_enabled)
        {
            dashboard_implausibility_clear(); // Drive re-enabled by the driver: the fault was acknowledged
        }
        send_calibration_cmd(buttons_data.values.drive_enabled ? CAL_CMD_DRIVE_ENABLE_ON : CAL_CMD_DRIVE_ENABLE_OFF);
    }
    if (buttons_data.values.traction_on != last_traction_on)
    {
        send_calibration_cmd(buttons_data.values.traction_on ? CAL_CMD_TRACTION_ON : CAL_CMD_TRACTION_OFF);
    }
    if (buttons_data.values.mode != last_mode)
    {
        send_calibration_cmd(buttons_data.values.mode ? CAL_CMD_MODE_RACE : CAL_CMD_MODE_NORMAL);
    }
    if (buttons_data.values.telemetry_enabled != last_telemetry_enabled)
    {
        send_calibration_cmd(buttons_data.values.telemetry_enabled ? CAL_CMD_TELEMETRY_ENABLE : CAL_CMD_TELEMETRY_DISABLE);
    }


    last_drive_enabled = buttons_data.values.drive_enabled;
    last_traction_on = buttons_data.values.traction_on;
    last_mode = buttons_data.values.mode;
    last_telemetry_enabled = buttons_data.values.telemetry_enabled;

    if (dashboard_data.cal_tps_0 && !last_cal_tps_0)
    {
        send_calibration_cmd(CAL_CMD_TPS_0);
    }
    if (dashboard_data.cal_tps_100 && !last_cal_tps_100)
    {
        send_calibration_cmd(CAL_CMD_TPS_100);
    }
    if (dashboard_data.cal_left_steer && !last_cal_left_steer)
    {
        send_calibration_cmd(CAL_CMD_LEFT_STEER);
    }
    if (dashboard_data.cal_center_steer && !last_cal_center_steer)
    {
        send_calibration_cmd(CAL_CMD_CENTER_STEER);
    }
    if (dashboard_data.cal_right_steer && !last_cal_right_steer)
    {
        send_calibration_cmd(CAL_CMD_RIGHT_STEER);
    }
    if (dashboard_data.cal_current_sensors && !last_cal_current_sensors)
    {
        send_calibration_cmd(CAL_CMD_CURRENT_SENSORS);
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
