#include "test_data.h"

static bool s_test_data_enabled = false;
static uint32_t s_test_tick = 0;

void test_data_set_enabled(bool enabled)
{
    s_test_data_enabled = enabled;
    if(!enabled)
    {
        s_test_tick = 0;
    }
}

bool test_data_is_enabled(void)
{
    return s_test_data_enabled;
}

void test_data_toggle(void)
{
    test_data_set_enabled(!s_test_data_enabled);
}

void test_data_update(dashboard_data_t *d)
{
    if(!s_test_data_enabled || d == 0)
    {
        return;
    }

    s_test_tick++;
    uint32_t phase = s_test_tick % 600U;

    float base_speed = 1000.0f + (float)(phase % 200U) * 4.0f; // 1000..1796 rpm
    float front_avg = base_speed;
    float rear_avg = base_speed;

    // Inject rear/front mismatch windows for visualization.
    if((phase >= 180U && phase < 240U) || (phase >= 420U && phase < 480U))
    {
        rear_avg = base_speed * 1.35f;
    }

    d->wheel_speed_fl = front_avg - 8.0f;
    d->wheel_speed_fr = front_avg + 8.0f;
    d->wheel_speed_rl = rear_avg - 10.0f;
    d->wheel_speed_rr = rear_avg + 10.0f;

    d->rpm = (d->wheel_speed_rl + d->wheel_speed_rr) * 0.5f;

    d->tps = (float)(phase % 100U);
    d->tps_1 = d->tps + 1.0f;
    d->tps_2 = d->tps - 1.0f;

    d->brake_front = (phase >= 300U && phase < 360U) ? 62.0f : 8.0f;
    d->brake_rear = (phase >= 300U && phase < 360U) ? 55.0f : 6.0f;

    d->steering_angle = (float)((int)(phase % 90U) - 45);

    d->driver1_dc_voltage = 312.0f;
    d->driver2_dc_voltage = 309.0f;
    d->driver1_dc_current = 42.0f;
    d->driver2_dc_current = 39.0f;
    d->motor1_ac_current = 74.0f;
    d->motor2_ac_current = 69.0f;
    d->motor1_temp = 52.0f;
    d->motor2_temp = 50.0f;

    d->battery_voltage = (d->driver1_dc_voltage + d->driver2_dc_voltage) * 0.5f;
    d->battery_current = d->driver1_dc_current + d->driver2_dc_current;

    // Keep traction ON in visualization mode.
    d->traction_on = 1;

    d->canopen_state = ((phase >= 20U && phase < 40U) ? PRE_OPERATIONAL : OPERATIONAL);

    // Cycle warnings and faults for both drivers.
    d->driver1_warning = (phase >= 90U && phase < 150U) ? CONTROLLER_TEMPERATURE_EXCEEDED : NO_WARNING;
    d->driver2_warning = (phase >= 230U && phase < 290U) ? DC_LINK_OVERVOLTAGE : NO_WARNING;

    d->driver1_error = (phase >= 330U && phase < 380U) ? (uint8_t)(ERROR_CURRENT_A & 0xFF) : NO_FAULT;
    d->driver2_error = (phase >= 500U && phase < 550U) ? (uint8_t)(ERROR_STO_ERROR & 0xFF) : NO_FAULT;
}
