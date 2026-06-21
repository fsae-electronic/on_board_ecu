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

#define MAIN_ECU_ID 0x401   

#define ON_BOARD_ECU_ID 0x402


typedef struct
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
} tps_data_t;


typedef struct
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint16_t front_left_speed; // Front left wheel speed
            uint16_t front_right_speed; // Front right wheel speed
            uint16_t direction; // Angle of the front wheels (0-360 degrees)
            uint16_t front_brake_pressure; // Front brake pressure
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = FRONT_DATA_ID;
} front_data_t;

typedef struct
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
} current_data_t;

typedef struct
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
} rear_data_t;

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
            uint16_t dc_voltage;
            uint16_t rated_current;
            uint8_t temp;
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
            uint8_t driver_temp; // Driver temperature
            uint16_t driver_voltage; // Driver voltage
        } values;
    };
    volatile bool new_data;
    uint16_t can_id;
};


typedef struct
{
    union
    {
        uint8_t raw[8];

        struct
        {
            uint8_t tps;
            uint8_t mode;
            uint8_t error_code; // Internal error code
        } values;
    };
    volatile bool new_data;
    uint16_t can_id = MAIN_ECU_ID; 
} main_ecu_data_t;


typedef struct
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
} buttons_data_t;


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

extern main_ecu_data_t main_ecu_data;

extern buttons_data_t buttons_data;



void init_data(void);
void update_data(void);

#endif /* SOURCE_DATA_H_ */
