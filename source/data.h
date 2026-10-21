/*
 * data.h
 *
 *  Created on: 13 mar. 2026
 *      Author: agust
 */

#ifndef SOURCE_DATA_H_
#define SOURCE_DATA_H_

#include <stdint.h>
#include <stdbool.h>


#define TPS_ID 0x501
#define FRONT_DATA_ID 0x502
#define CURRENT_DATA_ID 0x503
#define REAR_DATA_ID 0x504

#define DRIVER1_STATUS_ID 0x181   //TPDO1 Driver 1
#define DRIVER2_STATUS_ID 0x182   //TPDO1 Driver 2

#define MOTOR1_DATA_ID 0x281    //TPDO2 Driver 1
#define MOTOR2_DATA_ID 0x282    //TPDO2 Driver 2

#define DRIVER1_DATA_ID 0x381   //TPDO3 Driver 1
#define DRIVER2_DATA_ID 0x382   //TPDO3 Driver 2

#define IMPLAUSIBILITY_ID 0x401  // Sent once per fault by adc2can_front / adc2can_rear

#define ON_BOARD_ECU_ID 0x402
#define MAIN_ECU_ERRORS_ID 0x403 // Main ECU errors (EV.4.7, sensor CAN timeouts), on change and every 100 ms while active
#define CALIBRATION_CMD_ID 0x600
#define CANOPEN_HEARTBEAT_ID 0x700

#define CHARGER_CMD_ID 0x610    // Charging command from main ECU (every 500 ms while charging)
#define CHARGER_DATA_ID 0x611   // Charging data from main ECU (every 500 ms while charging)

// If no 0x610 frame arrives within this time, charging is considered finished.
#define CHARGER_TIMEOUT_TICKS (60 * 2) // 2 s at the 60 Hz update_data() rate

// If a main ECU error is active and no 0x403 frame arrives within this time, it is cleared.
#define MAIN_ECU_ERRORS_TIMEOUT_TICKS 18 // ~300 ms at the 60 Hz update_data() rate

enum calibration_cmd_id_t
{
    CAL_CMD_NONE = 0,
    CAL_CMD_TPS_0 = 1,
    CAL_CMD_TPS_100 = 2,
    CAL_CMD_LEFT_STEER = 3,
    CAL_CMD_CENTER_STEER = 4,
    CAL_CMD_RIGHT_STEER = 5,
    CAL_CMD_CURRENT_SENSORS = 6,
    CAL_CMD_DRIVE_ENABLE_ON = 7,
    CAL_CMD_DRIVE_ENABLE_OFF = 8,
    CAL_CMD_TRACTION_ON = 9,
    CAL_CMD_TRACTION_OFF = 0xA,
    CAL_CMD_MODE_NORMAL = 0xB,
    CAL_CMD_MODE_RACE = 0xC,
    CAL_CMD_TELEMETRY_ENABLE = 0xD,
    CAL_CMD_TELEMETRY_DISABLE = 0xE
};


struct tps_data_t
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint16_t tps_1; // Throttle position sensor 1
            uint16_t tps_2; // Throttle position sensor 2
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = TPS_ID;
};


struct front_data_t
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint16_t front_left_speed; // Front left wheel speed
            uint16_t front_right_speed; // Front right wheel speed
            int16_t direction; // Steering angle in degrees, signed (negative = left of center)
            uint16_t front_brake_pressure; // Front brake pressure
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = FRONT_DATA_ID;
};

struct current_data_t
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint16_t ac_current_1; // AC current sensor 1
            uint16_t ac_current_2; // AC current sensor 2
            uint16_t dc_current_1; // DC current sensor 1
            uint16_t dc_current_2; // DC current sensor 2
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = CURRENT_DATA_ID;
};

struct rear_data_t
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint16_t rear_brake_pressure; // Rear brake pressure
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = REAR_DATA_ID;
};

struct driver_status_t
{
    explicit driver_status_t(uint16_t id) : can_id(id) {}

    union
    {
        uint8_t raw[8];
        struct
        {
            uint16_t status_word;
            uint16_t warning;
            uint16_t error;
        } values;
    };
    volatile bool new_data;
    uint16_t can_id;
};

//TPDO2
struct motor_data_t
{
    explicit motor_data_t(uint16_t id) : can_id(id) {}

    union
    {
        uint8_t raw[8];
        struct
        {
            int32_t motor_velocity;      // 0x606C:0 Velocity_actual_value (RPM)
            uint16_t driver_dc_voltage;  // 0x6079:0 DC_link_circuit_voltage
        } values;
    };
    volatile bool new_data;
    uint16_t can_id;
};

//TPDO3
struct driver_data_t
{
    explicit driver_data_t(uint16_t id) : can_id(id) {}

    union
    {
        uint8_t raw[8];

        struct
        {
            uint32_t motor_rated_current; // 0x6075:0 Motor_rated_current
            uint8_t driver_temp;          // 0x2026:1 Controller temperature
            uint8_t motor_temp;           // 0x2025:0 Motor temperature
        } values;
    };
    volatile bool new_data;
    uint16_t can_id;
};


// 0x401: implausibility reported by adc2can_front / adc2can_rear (one-shot, not periodic).
// The main ECU forces drive off when it receives it and stays stopped until a new CAL_CMD_DRIVE_ENABLE_ON.
struct implausibility_data_t
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint8_t tps_implausibility;   // 1 = TPS implausibility, 0 = no
            uint8_t brake_implausibility; // 1 = brake implausibility, 0 = no
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = IMPLAUSIBILITY_ID;
};


// 0x403: errors reported by the main ECU. Sent on every change and every 100 ms while b0 or b1 != 0.
// Not sent periodically when there is no error.
struct main_ecu_errors_t
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint8_t apps_brake_plausibility; // 1 = EV.4.7 APPS/brake plausibility active (motors stopped), 0 = no
            uint8_t sensor_can_timeout;      // SENSOR_TIMEOUT_* bitmask (drive forced off while != 0)
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = MAIN_ECU_ERRORS_ID;
};


struct buttons_data_t
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint8_t drive_enabled; // bit 0: Drive enabled
            uint8_t traction_on;   // bit 1: Traction control on
            uint8_t mode;          // bit 2: Mode (0 = Normal, 1 = Race)
            uint8_t telemetry_enabled; // bit 3: Telemetry enabled
        } values;
    };
    volatile bool new_data;
};

struct canopen_heartbeat_t
{
    union
    {
        uint8_t raw[8];
        struct
        {
            uint8_t canopen_state;
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = CANOPEN_HEARTBEAT_ID;
};

struct calibration_cmd_t
{
    union
    {
        uint8_t raw[8];
        struct
        {
            uint8_t cmd_id;
            uint8_t value; // used by CAL_CMD_TPS_0..CAL_CMD_CURRENT_SENSORS only; button state commands are one-shot (value = 0)
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = CALIBRATION_CMD_ID;
};


struct charger_cmd_t
{
    union
    {
        uint8_t raw[8];
        struct
        {
            uint8_t charging; // 1 = charging, 0 = not charging
            uint8_t flags;    // Status flags reported by the charger
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = CHARGER_CMD_ID;
};

struct charger_data_t
{
    union
    {
        uint8_t raw[8];
        struct
        {
            uint16_t cmd_voltage;  // Commanded voltage (0.1 V)
            uint16_t cmd_current;  // Commanded current (0.1 A)
            uint16_t meas_voltage; // Voltage reported by the charger (0.1 V)
            uint16_t meas_current; // Current reported by the charger (0.1 A)
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = CHARGER_DATA_ID;
};


extern tps_data_t tps_data;
extern front_data_t front_data;

extern current_data_t current_data;
extern rear_data_t rear_data;

extern driver_status_t driver1_status;
extern driver_status_t driver2_status;

extern motor_data_t motor1_data;
extern motor_data_t motor2_data;

extern driver_data_t driver1_data;
extern driver_data_t driver2_data;

extern implausibility_data_t implausibility_data;
extern main_ecu_errors_t main_ecu_errors;

extern buttons_data_t buttons_data;
extern canopen_heartbeat_t canopen_heartbeat;
extern calibration_cmd_t calibration_cmd;

extern charger_cmd_t charger_cmd;
extern charger_data_t charger_data;



void init_data(void);
void update_data(void);

#endif /* SOURCE_DATA_H_ */
