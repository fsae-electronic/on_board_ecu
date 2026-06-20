#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "Bridgetek_EVE2.hpp"
#include <stdint.h>

enum driver_status_word_t
{
    READY_TO_SWITCH_ON = 0,
    SWITCHED_ON = 1,
    OPERATION_ENABLED = 2,
    FAULT = 4,
    VOLTAGE_ENABLED = 8,
    QUICK_STOP = 16,
    SWITCH_ON_DISABLED = 32,
    WARNING = 64,
    MFR_SPECIFIC = 128,
    REMOTE = 256,
    TARGET_REACHED = 512,
    INTERNAL_LIMIT_ACTIVE = 1024,
    OTHER_STATUS = 2048
};

enum driver_warning_code_t
{
    NO_WARNING = 1,
    CONTROLLER_TEMPERATURE_EXCEEDED = 2,
    MOTOR_TEMPERATURE_EXCEEDED = 4,
    DC_LINK_UNDERVOLTAGE = 8,
    DC_LINK_OVERVOLTAGE = 16,
    STALL_PROTECTION = 32,
    MAX_VELOCITY_EXCEEDED = 64,
    BMS_PROPOSED_POWER = 128,
    CAPACITOR_TEMPERATURE_EXCEEDED = 256,
    I2T_PROTECTION = 512,
    FIELD_WEAKNESS = 1024,
    OTHER_WARNING = 2048
};

enum driver_error_code_t
{
    NO_FAULT = 0,
    ERROR_CURRENT_A = 0xFF01,
    ERROR_CURRENT_B = 0xFF02,
    ERROR_HS_FET = 0xFF03,
    ERROR_LS_FET = 0xFF04,
    ERROR_DRV_LS_L1 = 0xFF05,
    ERROR_DRV_LS_L2 = 0xFF06,
    ERROR_DRV_LS_L3 = 0xFF07,
    ERROR_DRV_HS_L1 = 0xFF08,
    ERROR_DRV_HS_L2 = 0xFF09,
    ERROR_DRV_HS_L3 = 0xFF0A,
    ERROR_MOTOR_FEEDBACK = 0xFF0B,
    ERROR_DC_LINK_UNDERVOLTAGE = 0xFF0C,
    ERROR_PULS_MODE_FINISHED = 0xFF0D,
    ERROR_APP_ERROR = 0xFF0E,
    ERROR_STO_ERROR = 0xFF0F,
    ERROR_CONTROLLER_OVERTEMPERATURE = 0xFF10,
    ERROR_DC_LINK_OVERVOLTAGE = 0x3210,
    ERROR_UNKNOWN = 0xFFFF
};




enum main_ecu_error_code_t
{
    MAIN_ECU_NO_ERROR = 0,
    MAIN_ECU_TPS_IMPLAUSIBILITY,
    MAIN_ECU_BREAK_IMPLAUSIBILITY,
    MAIN_ECU_SAFETY_SHUTDOWN,
    MAIN_ECU_OTHER_ERROR
};


enum drive_state_t
{
    OFF = 0,
    ON,
    DRIVE_FAULT
};

enum mode_t
{
    NORMAL= 0,
    RACE
};



typedef struct
{
    float rpm;
    
    float tps;
    float tps_1;
    float tps_2;


    float motor1_dc_voltage;
    float motor1_dc_current;
    float motor1_ac_current;
    float motor1_temp;

    float motor2_dc_voltage;
    float motor2_dc_current;
    float motor2_ac_current;
    float motor2_temp;

    float brake_front;
    float brake_rear;
    float steering_angle;      // degrees

    // telemetry additions
    float battery_voltage;
    float battery_current;
    
    float wheel_speed_fl;      // front left
    float wheel_speed_fr;      // front right
    float wheel_speed_rl;      // rear left
    float wheel_speed_rr;      // rear right

    float driver1_temp;
    float driver1_voltage;

    float driver2_temp;
    float driver2_voltage;

    uint8_t driver1_warning;
    uint8_t driver1_error;
    uint8_t driver2_warning;
    uint8_t driver2_error;

    
    //Buttons and modes
    uint8_t drive_state;
    uint8_t drive_enabled;
    uint8_t traction_on;
    uint8_t telemetry_enabled;
    uint8_t mode;

    uint8_t cal_tps_0;
    uint8_t cal_tps_100;
    uint8_t cal_left_steer;
    uint8_t cal_right_steer;

    uint8_t cal_screen;




    // historical data for graphs (circular buffer)
    
    #define GRAPH_BUFFER_SIZE 256
    float motor1_dc_voltage_history[GRAPH_BUFFER_SIZE];
    float motor1_dc_current_history[GRAPH_BUFFER_SIZE];
    float motor1_ac_current_history[GRAPH_BUFFER_SIZE];
    float motor1_temp_history[GRAPH_BUFFER_SIZE];
    
    float motor2_dc_voltage_history[GRAPH_BUFFER_SIZE];
    float motor2_dc_current_history[GRAPH_BUFFER_SIZE];
    float motor2_ac_current_history[GRAPH_BUFFER_SIZE];
    float motor2_temp_history[GRAPH_BUFFER_SIZE];
    
    float steering_angle_history[GRAPH_BUFFER_SIZE];
    float tps_history[GRAPH_BUFFER_SIZE];
    float brake_front_history[GRAPH_BUFFER_SIZE];
    float brake_rear_history[GRAPH_BUFFER_SIZE];
    
    float wheel_speed_fl_history[GRAPH_BUFFER_SIZE];
    float wheel_speed_fr_history[GRAPH_BUFFER_SIZE];
    float wheel_speed_rl_history[GRAPH_BUFFER_SIZE];
    float wheel_speed_rr_history[GRAPH_BUFFER_SIZE];
    
    int graph_buffer_index;

} dashboard_data_t;

extern dashboard_data_t dashboard_data;
extern int cal_tps_0_timer;
extern int cal_tps_100_timer;
extern int cal_left_steer_timer;
extern int cal_right_steer_timer;   


void init_dashboard(dashboard_data_t *data);
void update_dashboard_data(dashboard_data_t *data);
void update_dashboard_draw(Bridgetek_EVE2 &eve, dashboard_data_t *data);

#endif // DASHBOARD_H
