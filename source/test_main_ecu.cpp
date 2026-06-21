#include "test_main_ecu.hpp"



// simulated values
static float rpm = 0;

static float battery_voltage = 400;
static float battery_current = 20;

static float tps = 0;
static float brake_front = 0;
static float brake_rear = 0;
static float steering_angle = 0;

static float wheel_speed_fl = 0;
static float wheel_speed_fr = 0;
static float wheel_speed_rl = 0;
static float wheel_speed_rr = 0;

static float motor_voltage = 400;
static float motor_current = 20;
static float motor_temp = 40;

static uint8_t motor1_fault = 0;
static uint8_t motor2_fault = 0;
static uint8_t canopen_state = 0;

static uint32_t sim_counter = 0;


// buffers
static uint8_t sensors_msg[8];
static uint8_t wheel_speed_msg[8];
static uint8_t dynamic_msg[8];
static uint8_t motor1_msg[8];
static uint8_t motor2_msg[8];
static uint8_t status_msg[8];


void test_main_ecu_init(void)
{
    tps = 0;
    brake_front = 0;
    brake_rear = 0;
    steering_angle = 0;
}


void test_main_ecu_periodic(void)
{
    // -------- SIMULATED SENSOR VARIATION --------
    rpm += 10;
    if (rpm > 5000) rpm = 0;

    battery_voltage += 0.5;
    if (battery_voltage > 400) battery_voltage = 350;

    battery_current += 1;
    if (battery_current > 200) battery_current = 0;


    steering_angle += 1;
    if (steering_angle > 90) steering_angle = -90;

    tps += 1;
    if (tps > 100) tps = 0;

    brake_front += 0.5;
    if (brake_front > 100) brake_front = 0;

    brake_rear += 0.3;
    if (brake_rear > 100) brake_rear = 0;

    wheel_speed_fl += 1;
    wheel_speed_fr += 1.2;
    wheel_speed_rl += 0.8;
    wheel_speed_rr += 0.9;

    if (wheel_speed_fl > 250) wheel_speed_fl = 0;
    if (wheel_speed_fr > 250) wheel_speed_fr = 0;
    if (wheel_speed_rl > 250) wheel_speed_rl = 0;
    if (wheel_speed_rr > 250) wheel_speed_rr = 0;

    motor_voltage += 0.1;
    if (motor_voltage > 420) motor_voltage = 380;

    motor_current += 0.05;
    if (motor_current > 60) motor_current = 40;

    motor_temp += 0.1;
    if (motor_temp > 90) motor_temp = 40;


    // -------- FAULT SIMULATION --------

    sim_counter++;

    if ((sim_counter % 1000) == 0)
    {
        motor1_fault = (motor1_fault + 1) % 6;
        motor2_fault = (motor2_fault + 1) % 6;
        canopen_state = (canopen_state + 1) % 3;
    }


    // -------- PACK DATA --------

    uint16_t *s = (uint16_t*)sensors_msg;
    s[0] = (uint16_t)tps;
    s[1] = (uint16_t)brake_front;
    s[2] = (uint16_t)brake_rear;
    s[3] = (uint16_t)(steering_angle + 180); // avoid negatives


    uint16_t *w = (uint16_t*)wheel_speed_msg;
    w[0] = (uint16_t)wheel_speed_fl;
    w[1] = (uint16_t)wheel_speed_fr;
    w[2] = (uint16_t)wheel_speed_rl;
    w[3] = (uint16_t)wheel_speed_rr;

    uint16_t *d = (uint16_t*)dynamic_msg;
    d[0] = (uint16_t)rpm;
    d[1] = (uint16_t)battery_voltage;
    d[2] = (uint16_t)battery_current;
    d[3] = 0;



    uint16_t *m1 = (uint16_t*)motor1_msg;
    m1[0] = (uint16_t)motor_voltage;
    m1[1] = (uint16_t)motor_current;
    m1[2] = (uint16_t)motor_temp;
    m1[3] = 0;


    uint16_t *m2 = (uint16_t*)motor2_msg;
    m2[0] = (uint16_t)motor_voltage;
    m2[1] = (uint16_t)motor_current;
    m2[2] = (uint16_t)motor_temp;
    m2[3] = 0;


    status_msg[0] = motor1_fault;
    status_msg[1] = motor2_fault;
    status_msg[2] = canopen_state;
    status_msg[3] = 0;
    status_msg[4] = 0;
    status_msg[5] = 0;
    status_msg[6] = 0;
    status_msg[7] = 0;


    // -------- CAN TRANSMIT --------

    canTransmit(canREG1, canMESSAGE_BOX17, sensors_msg);
    canTransmit(canREG1, canMESSAGE_BOX18, wheel_speed_msg);
    canTransmit(canREG1, canMESSAGE_BOX19, motor1_msg);
    canTransmit(canREG1, canMESSAGE_BOX20, motor2_msg);
    canTransmit(canREG1, canMESSAGE_BOX21, status_msg);
    canTransmit(canREG1, canMESSAGE_BOX22, dynamic_msg);
}
