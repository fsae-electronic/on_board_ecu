#include "dashboard.h"
#include "ui_rpm_bar.h"
#include "ui_buttons.h"
#include "pages.h"
#include "ui_touch.h"
#include "test_data.h"
#include <stdio.h>
extern "C" {
#include "sci.h"
}

#include <string.h>

#include <math.h>
#include <stdio.h>


dashboard_data_t dashboard_data;

int cal_tps_0_timer = 0;
int cal_tps_100_timer = 0;
int cal_left_steer_timer = 0;
int cal_center_steer_timer = 0;
int cal_right_steer_timer = 0;
int cal_current_sensors_timer = 0;

#define DASHBOARD_LOG_CAPACITY 96
#define DASHBOARD_LOG_VISIBLE_LINES 9

typedef struct
{
    uint32_t tick;
    uint8_t driver;
    log_level_t level;
    char message[54];
} dashboard_log_entry_t;

static dashboard_log_entry_t s_logs[DASHBOARD_LOG_CAPACITY];
static uint16_t s_log_count = 0;
static uint16_t s_log_head = 0;
static uint16_t s_log_scroll = 0;
static uint32_t s_log_tick = 0;

enum race_overlay_t
{
    RACE_OVERLAY_NONE = 0,
    RACE_OVERLAY_WARNING,
    RACE_OVERLAY_FAULT
};

static race_overlay_t s_race_overlay = RACE_OVERLAY_NONE;
static uint8_t s_overlay_driver = 0;
static uint8_t s_overlay_code = 0;

// Remembers the last dismissed fault/warning so it stays hidden until it clears or a different one appears.
static bool s_fault_acked = false;
static uint8_t s_acked_fault_driver = 0;
static uint8_t s_acked_fault_code = 0;
static bool s_warn_acked = false;
static uint8_t s_acked_warn_driver = 0;
static uint8_t s_acked_warn_code = 0;

static const char *warning_to_str(uint8_t warning)
{
    switch(warning)
    {
        case NO_WARNING: return "NO_WARN";
        case CONTROLLER_TEMPERATURE_EXCEEDED: return "CTRL_TEMP";
        case MOTOR_TEMPERATURE_EXCEEDED: return "MTR_TEMP";
        case DC_LINK_UNDERVOLTAGE: return "DC_UV";
        case DC_LINK_OVERVOLTAGE: return "DC_OV";
        case STALL_PROTECTION: return "STALL";
        case MAX_VELOCITY_EXCEEDED: return "MAX_VEL";
        case BMS_PROPOSED_POWER: return "BMS_LIM";
        // default: return "WARN_UNK";
    }
}

static const char *error_to_str(uint8_t error)
{
    if(error == (uint8_t)NO_FAULT) return "NO_FAULT";

    switch((uint16_t)(0xFF00U | error))
    {
        case ERROR_CURRENT_A: return "CUR_A";
        case ERROR_CURRENT_B: return "CUR_B";
        case ERROR_HS_FET: return "HS_FET";
        case ERROR_LS_FET: return "LS_FET";
        case ERROR_DRV_LS_L1: return "DRV_LS_L1";
        case ERROR_DRV_LS_L2: return "DRV_LS_L2";
        case ERROR_DRV_LS_L3: return "DRV_LS_L3";
        case ERROR_DRV_HS_L1: return "DRV_HS_L1";
        case ERROR_DRV_HS_L2: return "DRV_HS_L2";
        case ERROR_DRV_HS_L3: return "DRV_HS_L3";
        case ERROR_MOTOR_FEEDBACK: return "MTR_FB";
        case ERROR_DC_LINK_UNDERVOLTAGE: return "DC_UV";
        case ERROR_PULS_MODE_FINISHED: return "PLS_END";
        case ERROR_APP_ERROR: return "APP_ERR";
        case ERROR_STO_ERROR: return "STO_ERR";
        case ERROR_CONTROLLER_OVERTEMPERATURE: return "CTRL_TEMP";
        // default: return "ERR_UNK";
    }
}

static const char *canopen_to_str(uint8_t canopen_state)
{
    switch(canopen_state)
    {
        case BOOTUP: return "BOOT";
        case PRE_OPERATIONAL: return "PRE_OP";
        case OPERATIONAL: return "OP";
        case STOPPED: return "STOP";
        default: return "STOP_UNK";
    }
}

static bool race_find_fault(const dashboard_data_t *d, uint8_t *driver, uint8_t *code)
{
    if(d->driver1_error != NO_FAULT)
    {
        *driver = 1;
        *code = d->driver1_error;
        return true;
    }
    if(d->driver2_error != NO_FAULT)
    {
        *driver = 2;
        *code = d->driver2_error;
        return true;
    }
    return false;
}

static bool race_find_warning(const dashboard_data_t *d, uint8_t *driver, uint8_t *code)
{
    if(d->driver1_warning != NO_WARNING)
    {
        *driver = 1;
        *code = d->driver1_warning;
        return true;
    }
    if(d->driver2_warning != NO_WARNING)
    {
        *driver = 2;
        *code = d->driver2_warning;
        return true;
    }
    return false;
}

// Charging overlay: shown on every page while charging. Touching it hides it for
// CHARGING_SNOOZE_FRAMES, after which it comes back if charging is still active.
#define DASHBOARD_DRAW_HZ 30
#define CHARGING_SNOOZE_FRAMES (DASHBOARD_DRAW_HZ * 8)
// Pack voltage shown as 0 %. 100 % is the voltage commanded to the charger (0x611 B0-B1).
#define CHARGE_EMPTY_VOLTAGE 72.5f

static bool s_charging_overlay = false;
static uint16_t s_charging_snooze = 0;
static uint32_t s_charging_anim = 0;

// Brake alert: raised when drive is requested without the brake pressed. Closed by a
// touch or automatically once the front brake exceeds DRIVE_ENABLE_MIN_BRAKE_PSI.
static bool s_brake_alert = false;

void dashboard_brake_alert_raise(void)
{
    if(!s_brake_alert)
    {
        dashboard_log_event(0, LOG_WARNING, "Drive blocked: press brake");
    }
    s_brake_alert = true;
}

bool dashboard_brake_alert_is_active(void)
{
    return s_brake_alert;
}

void dashboard_brake_alert_acknowledge(void)
{
    s_brake_alert = false;
}

// Implausibility alert (0x401): the main ECU forced drive off. The frame is one-shot, so the
// alert stays up until it is closed by a touch or the driver re-enables drive.
static uint8_t s_implausibility = 0;

void dashboard_implausibility_raise(uint8_t flags)
{
    uint8_t added = (uint8_t)(flags & (uint8_t)~s_implausibility);
    if(added & IMPLAUSIBILITY_TPS)
    {
        dashboard_log_event(0, LOG_FAULT, "TPS implausibility: drive OFF");
    }
    if(added & IMPLAUSIBILITY_BRAKE)
    {
        dashboard_log_event(0, LOG_FAULT, "Brake implausibility: drive OFF");
    }
    s_implausibility |= flags;
}

void dashboard_implausibility_clear(void)
{
    s_implausibility = 0;
}

bool dashboard_implausibility_is_active(void)
{
    return s_implausibility != 0;
}

bool dashboard_charging_overlay_is_active(void)
{
    return s_charging_overlay;
}

void dashboard_charging_overlay_acknowledge(void)
{
    s_charging_overlay = false;
    s_charging_snooze = CHARGING_SNOOZE_FRAMES;
}

bool dashboard_race_overlay_is_active(void)
{
    return s_race_overlay != RACE_OVERLAY_NONE;
}

void dashboard_race_overlay_acknowledge(void)
{
    if(s_race_overlay == RACE_OVERLAY_FAULT)
    {
        s_fault_acked = true;
        s_acked_fault_driver = s_overlay_driver;
        s_acked_fault_code = s_overlay_code;
    }
    else if(s_race_overlay == RACE_OVERLAY_WARNING)
    {
        s_warn_acked = true;
        s_acked_warn_driver = s_overlay_driver;
        s_acked_warn_code = s_overlay_code;
    }
    s_race_overlay = RACE_OVERLAY_NONE;
    s_overlay_driver = 0;
    s_overlay_code = 0;
}

static void format_log_timestamp(uint32_t tick, char *out)
{
    uint32_t total_ms = (tick * 1000U) / 60U;
    uint32_t minutes = total_ms / 60000U;
    uint32_t seconds = (total_ms / 1000U) % 60U;
    uint32_t millis = total_ms % 1000U;
    sprintf(out, "%02lu:%02lu.%03lu", (unsigned long)minutes, (unsigned long)seconds, (unsigned long)millis);
}

static uint16_t log_max_scroll(void)
{
    if(s_log_count <= DASHBOARD_LOG_VISIBLE_LINES)
    {
        return 0;
    }
    return (uint16_t)(s_log_count - DASHBOARD_LOG_VISIBLE_LINES);
}

static bool log_get_by_order(uint16_t ordered_index, dashboard_log_entry_t *entry)
{
    if(ordered_index >= s_log_count)
    {
        return false;
    }

    uint16_t oldest = (s_log_count == DASHBOARD_LOG_CAPACITY) ? s_log_head : 0;
    uint16_t physical = (uint16_t)((oldest + ordered_index) % DASHBOARD_LOG_CAPACITY);
    *entry = s_logs[physical];
    return true;
}

void dashboard_log_event(uint8_t driver, log_level_t level, const char *message)
{
    if(message == NULL)
    {
        return;
    }

    dashboard_log_entry_t *entry = &s_logs[s_log_head];
    entry->tick = s_log_tick;
    entry->driver = driver;
    entry->level = level;
    strncpy(entry->message, message, sizeof(entry->message) - 1);
    entry->message[sizeof(entry->message) - 1] = '\0';

    s_log_head = (uint16_t)((s_log_head + 1U) % DASHBOARD_LOG_CAPACITY);
    if(s_log_count < DASHBOARD_LOG_CAPACITY)
    {
        s_log_count++;
    }

    // Al ingresar eventos nuevos, volver al final (eventos más recientes)
    s_log_scroll = 0;

    char ts[20];
    char uart_line[128];
    const char *driver_txt = "SYS";
    const char *level_txt = "INFO";

    if(driver == 1) driver_txt = "DRV1";
    if(driver == 2) driver_txt = "DRV2";

    if(level == LOG_WARNING) level_txt = "WARN";
    if(level == LOG_FAULT) level_txt = "FAULT";

    format_log_timestamp(entry->tick, ts);
    sprintf(uart_line, "[%s] %s %s %s\r\n", ts, driver_txt, level_txt, entry->message);
    sciSend(sciREG, (uint32)strlen(uart_line), (uint8 *)uart_line);
}

void dashboard_logs_scroll(int delta)
{
    int next = (int)s_log_scroll + delta;
    int max_scroll = (int)log_max_scroll();

    if(next < 0)
    {
        next = 0;
    }
    if(next > max_scroll)
    {
        next = max_scroll;
    }

    s_log_scroll = (uint16_t)next;
}

void dashboard_logs_clear(void)
{
    s_log_count = 0;
    s_log_head = 0;
    s_log_scroll = 0;
}



char *dtostrf(double val, signed char width, unsigned char prec, char *sout)
{
    char fmt[20];

    // Crear formato tipo "%4.1f"
    sprintf(fmt, "%%%d.%df", width, prec);

    sprintf(sout, fmt, val);

    return sout;
}

void draw_motor_block(
    Bridgetek_EVE2 &eve,
    int x,
    const char *title,
    float v_dc,
    float i_dc,
    float i_ac,
    float temp
)
{
    eve.COLOR_RGB(0,200,255);

    eve.CMD_TEXT(x,100,26,0,title);

    eve.COLOR_RGB(255,255,255);

    eve.CMD_TEXT(x,120,26,0,"V_DC");
    eve.CMD_NUMBER(x+80,120,26,0,v_dc);
    
    eve.CMD_TEXT(x,140,26,0,"I_DC");
    eve.CMD_NUMBER(x+80,140,26,0,i_dc);

    eve.CMD_TEXT(x,160,26,0,"I_AC");
    eve.CMD_NUMBER(x+80,160,26,0,i_ac);

    eve.CMD_TEXT(x,180,26,0,"TEMP");
    eve.CMD_NUMBER(x+80,180,26,0,temp);

}

void draw_center(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    int x = 190;

    eve.CMD_TEXT(x,80,26,0,"TPS");
    eve.CMD_NUMBER(x+60,80,26,0,d->tps);
    eve.CMD_PROGRESS(x,100,100,15,0,d->tps,100);

    eve.CMD_TEXT(x,130,26,0,"FRONT BREAK");
    eve.CMD_NUMBER(x+100,130,26,0,d->brake_front);
    eve.CMD_PROGRESS(x,150,100,15,0,d->brake_front,300);

    eve.CMD_TEXT(x,180,26,0,"REAR BREAK");
    eve.CMD_NUMBER(x+100,180,26,0,d->brake_rear);
    eve.CMD_PROGRESS(x,200,100,15,0,d->brake_rear,300);
}

void draw_status(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    int x = 270;
    int y = 5;
    char line[64];


    eve.COLOR_RGB(255,255,0);
    sprintf(line, "W1: %s", warning_to_str(d->driver1_warning));
    eve.CMD_TEXT(x-65, y, 21, 0, line);

    eve.COLOR_RGB(255,0,0);
    sprintf(line, "E1: %s", error_to_str(d->driver1_error));
    eve.CMD_TEXT(x-65, y + 18, 21, 0, line);

    eve.COLOR_RGB(255,255,0);
    sprintf(line, "W2: %s", warning_to_str(d->driver2_warning));
    eve.CMD_TEXT(x+65, y, 21, 0, line);

    eve.COLOR_RGB(255,0,0);
    sprintf(line, "E2: %s", error_to_str(d->driver2_error));
    eve.CMD_TEXT(x+65, y + 18, 21, 0, line);

    eve.COLOR_RGB(255,255,255);
    sprintf(line, "CAN: %s", canopen_to_str(d->canopen_state));
    eve.CMD_TEXT(x, y + 36, 21, 0, line);

    eve.COLOR_RGB(255,255,255);

}

static void draw_race_core(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    ui_draw_rpm_bar(eve, d->rpm);



    const int tile_y = 146;
    const int tile_h = 108;
    const int tile_w = 108;
    const int gap = 8;
    const int start_x = 16;

    auto draw_tile = [&](int x, const char *label, int value, const char *unit, uint8_t r, uint8_t g, uint8_t b)
    {
        char text[28];

        eve.COLOR_RGB(18,18,18);
        eve.BEGIN(eve.BEGIN_RECTS);
        eve.VERTEX2F(x * 16, tile_y * 16);
        eve.VERTEX2F((x + tile_w) * 16, (tile_y + tile_h) * 16);
        eve.END();

        eve.COLOR_RGB(r, g, b);
        eve.CMD_TEXT(x + 8, tile_y + 9, 20, 0, label);

        sprintf(text, "%d", value);
        eve.CMD_TEXT(x + tile_w / 2, tile_y + 45, 31, Bridgetek_EVE2::OPT_CENTER, text);

        eve.COLOR_RGB(155,155,155);
        eve.CMD_TEXT(x + tile_w - 8, tile_y + tile_h - 20, 20, Bridgetek_EVE2::OPT_RIGHTX, unit);
    };

    draw_tile(start_x + 0 * (tile_w + gap), "VBAT", (int)d->battery_voltage, "V", 80, 210, 255);
    draw_tile(start_x + 1 * (tile_w + gap), "IBAT", (int)d->battery_current, "A", 80, 210, 255);
    draw_tile(start_x + 2 * (tile_w + gap), "TPS", (int)d->tps, "%", 80, 255, 130);
    draw_tile(start_x + 3 * (tile_w + gap), "BRAKE", (int)d->brake_front, "PSI", 255, 220, 60);
}

static void draw_race_classic(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    ui_draw_rpm_bar(eve,d->rpm);

    draw_motor_block(
        eve,
        50,
        "MOTOR 1",
        d->driver1_dc_voltage,
        d->driver1_dc_current,
        d->motor1_ac_current,
        d->motor1_temp
    );

    draw_motor_block(
        eve,
        340,
        "MOTOR 2",
        d->driver2_dc_voltage,
        d->driver2_dc_current,
        d->motor2_ac_current,
        d->motor2_temp
    );

    draw_center(eve,d);
    ui_draw_buttons(eve,d);
    draw_status(eve,d);
}

static void draw_telemetry_grid(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    const int cellW = 109;
    const int cellH = 60;
    int startX = 6;
    int startY = 0;

    eve.COLOR_RGB(255,255,255);

    auto textInCell = [&](int col, int row, const char *label, float value, const char *unit, int tag)
    {
        int x = startX + col * (cellW + 10);
        int y = startY + row * (cellH + 10);
        char buf[24];
        int iv = (int)value;

        eve.TAG(tag);
        eve.BEGIN(eve.BEGIN_RECTS);
        eve.COLOR_RGB(25,25,25);
        eve.VERTEX2F(x * 16, y * 16);
        eve.VERTEX2F((x + cellW) * 16, (y + cellH) * 16);
        eve.END();

        eve.COLOR_RGB(255,255,255);
        eve.CMD_TEXT(x + cellW / 2, y + 10, 22, Bridgetek_EVE2::OPT_CENTERX, label);

        if(unit[0])
        {
            sprintf(buf, "%d %s", iv, unit);
        }
        else
        {
            sprintf(buf, "%d", iv);
        }
        eve.CMD_TEXT(x + cellW / 2, y + 34, 22, Bridgetek_EVE2::OPT_CENTERX, buf);
        eve.TAG(255);
    };

    textInCell(0,0,"DRV1", d->driver1_dc_voltage, "VDC", 10);
    textInCell(0,1,"DRV1", d->driver1_dc_current, "ADC", 11);
    textInCell(0,2,"M1", d->motor1_ac_current, "AC", 12);
    textInCell(0,3,"M1", d->motor1_temp, "C", 13);

    textInCell(1,0,"DRV2", d->driver2_dc_voltage, "VDC", 14);
    textInCell(1,1,"DRV2", d->driver2_dc_current, "ADC", 15);
    textInCell(1,2,"M2", d->motor2_ac_current, "AC", 16);
    textInCell(1,3,"M2", d->motor2_temp, "C", 17);

    textInCell(2,0,"TPS", d->tps, "%", 18);
    textInCell(2,1,"DIR", d->steering_angle, "deg", 19);
    textInCell(2,2,"FBRK", d->brake_front, "%", 20);
    textInCell(2,3,"RBRK", d->brake_rear, "%", 21);

    textInCell(3,0,"FL", d->wheel_speed_fl, "rpm", 22);
    textInCell(3,1,"FR", d->wheel_speed_fr, "rpm", 23);
    textInCell(3,2,"RL", d->wheel_speed_rl, "rpm", 24);
    textInCell(3,3,"RR", d->wheel_speed_rr, "rpm", 25);
}

static void draw_warning_overlay(Bridgetek_EVE2 &eve, uint8_t driver, uint8_t warning_code)
{
    char line[64];
    const char *drv = (driver == 1) ? "DRV1" : "DRV2";

    eve.COLOR_RGB(255, 225, 0);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F(0, 70 * 16);
    eve.VERTEX2F(480 * 16, 202 * 16);
    eve.END();

    eve.COLOR_RGB(0, 0, 0);
    eve.CMD_TEXT(240, 92, 31, Bridgetek_EVE2::OPT_CENTER, "WARNING");

    sprintf(line, "%s %s", drv, warning_to_str(warning_code));
    eve.CMD_TEXT(240, 128, 28, Bridgetek_EVE2::OPT_CENTER, line);

    eve.CMD_TEXT(240, 166, 21, Bridgetek_EVE2::OPT_CENTER, "TOUCH TO CLOSE");
}

static void draw_fault_overlay(Bridgetek_EVE2 &eve, uint8_t driver, uint8_t error_code)
{
    char line[64];
    const char *drv = (driver == 1) ? "DRV1" : "DRV2";

    eve.COLOR_RGB(190, 0, 0);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F(0, 0);
    eve.VERTEX2F(480 * 16, 272 * 16);
    eve.END();

    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(240, 94, 31, Bridgetek_EVE2::OPT_CENTER, "FAULT");

    sprintf(line, "%s %s", drv, error_to_str(error_code));
    eve.CMD_TEXT(240, 132, 28, Bridgetek_EVE2::OPT_CENTER, line);

    eve.CMD_TEXT(240, 170, 21, Bridgetek_EVE2::OPT_CENTER, "TOUCH TO CLOSE");
}

static void draw_round_rect(Bridgetek_EVE2 &eve, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t r)
{
    eve.LINE_WIDTH(r * 16);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F((x0 + r) * 16, (y0 + r) * 16);
    eve.VERTEX2F((x1 - r) * 16, (y1 - r) * 16);
    eve.END();
}

static void draw_brake_alert_overlay(Bridgetek_EVE2 &eve, const dashboard_data_t *d)
{
    char line[40];

    eve.SAVE_CONTEXT();
    eve.TAG(255);

    // Same style as the fault overlay
    eve.COLOR_RGB(190, 0, 0);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F(0, 0);
    eve.VERTEX2F(480 * 16, 272 * 16);
    eve.END();

    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(240, 60, 31, Bridgetek_EVE2::OPT_CENTER, "PRESS BRAKE");
    eve.CMD_TEXT(240, 102, 28, Bridgetek_EVE2::OPT_CENTER, "Brake must be pressed to enable drive");

    // Live front brake pressure against the required threshold
    const int16_t x0 = 90, x1 = 390, y0 = 138, y1 = 162;
    float ratio = d->brake_front / DRIVE_ENABLE_MIN_BRAKE_PSI;
    if(ratio < 0.0f) ratio = 0.0f;
    if(ratio > 1.0f) ratio = 1.0f;
    int16_t fill_w = (int16_t)((float)(x1 - x0 - 8) * ratio);

    eve.COLOR_RGB(110, 0, 0);
    draw_round_rect(eve, x0, y0, x1, y1, 8);
    if(fill_w >= 8)
    {
        eve.COLOR_RGB(255, 255, 255);
        draw_round_rect(eve, x0 + 4, y0 + 4, x0 + 4 + fill_w, y1 - 4, 4);
    }

    sprintf(line, "FRONT BRAKE %d / %d PSI", (int)d->brake_front, (int)DRIVE_ENABLE_MIN_BRAKE_PSI);
    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(240, 186, 28, Bridgetek_EVE2::OPT_CENTER, line);

    eve.CMD_TEXT(240, 236, 21, Bridgetek_EVE2::OPT_CENTER, "TOUCH TO CLOSE");

    eve.RESTORE_CONTEXT();
}

static void draw_implausibility_overlay(Bridgetek_EVE2 &eve)
{
    const char *title = "IMPLAUSIBILITY";
    if(s_implausibility == IMPLAUSIBILITY_TPS)
    {
        title = "TPS IMPLAUSIBILITY";
    }
    else if(s_implausibility == IMPLAUSIBILITY_BRAKE)
    {
        title = "BRAKE IMPLAUSIBILITY";
    }

    eve.SAVE_CONTEXT();
    eve.TAG(255);

    // Same style as the brake alert
    eve.COLOR_RGB(190, 0, 0);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F(0, 0);
    eve.VERTEX2F(480 * 16, 272 * 16);
    eve.END();

    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(240, 60, 31, Bridgetek_EVE2::OPT_CENTER, title);
    if(s_implausibility == (IMPLAUSIBILITY_TPS | IMPLAUSIBILITY_BRAKE))
    {
        eve.CMD_TEXT(240, 102, 28, Bridgetek_EVE2::OPT_CENTER, "TPS + BRAKE");
    }
    eve.CMD_TEXT(240, 140, 28, Bridgetek_EVE2::OPT_CENTER, "Drive disabled by main ECU");
    eve.CMD_TEXT(240, 186, 28, Bridgetek_EVE2::OPT_CENTER, "Enable drive to restart");
    eve.CMD_TEXT(240, 236, 21, Bridgetek_EVE2::OPT_CENTER, "TOUCH TO CLOSE");

    eve.RESTORE_CONTEXT();
}

// EV.4.7 APPS/brake plausibility (0x403). Not closable by touch: it goes away by itself
// when the main ECU releases it (throttle below 5 %). Drive stays enabled.
static void draw_apps_brake_plausibility_overlay(Bridgetek_EVE2 &eve)
{
    eve.SAVE_CONTEXT();
    eve.TAG(255);

    // Same style as the fault overlay
    eve.COLOR_RGB(190, 0, 0);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F(0, 0);
    eve.VERTEX2F(480 * 16, 272 * 16);
    eve.END();

    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(240, 70, 31, Bridgetek_EVE2::OPT_CENTER, "APPS / BRAKE");
    eve.CMD_TEXT(240, 112, 31, Bridgetek_EVE2::OPT_CENTER, "PLAUSIBILITY");
    eve.CMD_TEXT(240, 170, 30, Bridgetek_EVE2::OPT_CENTER, "SOLTAR ACELERADOR");
    eve.CMD_TEXT(240, 226, 21, Bridgetek_EVE2::OPT_CENTER, "Motors stopped by main ECU");

    eve.RESTORE_CONTEXT();
}

// Sensor CAN timeout (0x403 b1). Not closable by touch: it goes away when the main ECU clears it.
// Drive stays off and has to be enabled again by the driver afterwards.
static void draw_sensor_can_timeout_overlay(Bridgetek_EVE2 &eve, uint8_t flags)
{
    char line[40];
    line[0] = '\0';
    if(flags & SENSOR_TIMEOUT_TPS)         strcat(line, "TPS");
    if(flags & SENSOR_TIMEOUT_FRONT_BRAKE) strcat(line, (line[0] != '\0') ? " / FRENO DEL" : "FRENO DEL");
    if(flags & SENSOR_TIMEOUT_REAR_BRAKE)  strcat(line, (line[0] != '\0') ? " / FRENO TRAS" : "FRENO TRAS");

    eve.SAVE_CONTEXT();
    eve.TAG(255);

    // Same style as the fault overlay
    eve.COLOR_RGB(190, 0, 0);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F(0, 0);
    eve.VERTEX2F(480 * 16, 272 * 16);
    eve.END();

    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(240, 60, 31, Bridgetek_EVE2::OPT_CENTER, "SENSOR CAN TIMEOUT");
    eve.CMD_TEXT(240, 112, 29, Bridgetek_EVE2::OPT_CENTER, line);
    eve.CMD_TEXT(240, 170, 28, Bridgetek_EVE2::OPT_CENTER, "Drive disabled by main ECU");
    eve.CMD_TEXT(240, 226, 21, Bridgetek_EVE2::OPT_CENTER, "Enable drive again once cleared");

    eve.RESTORE_CONTEXT();
}

// Formats a value with one decimal without relying on printf float support.
static void format_tenths(char *out, float value, const char *unit)
{
    int32_t t = (int32_t)(value * 10.0f + ((value < 0.0f) ? -0.5f : 0.5f));
    const char *sign = "";
    if(t < 0)
    {
        sign = "-";
        t = -t;
    }
    sprintf(out, "%s%ld.%ld %s", sign, (long)(t / 10), (long)(t % 10), unit);
}

static uint8_t charge_percent(const dashboard_data_t *d)
{
    float full = d->charge_cmd_voltage;
    if(full <= CHARGE_EMPTY_VOLTAGE)
    {
        return 0;
    }
    float pct = (d->charge_voltage - CHARGE_EMPTY_VOLTAGE) * 100.0f / (full - CHARGE_EMPTY_VOLTAGE);
    if(pct < 0.0f) pct = 0.0f;
    if(pct > 100.0f) pct = 100.0f;
    return (uint8_t)(pct + 0.5f);
}

static void draw_charge_tile(Bridgetek_EVE2 &eve, int16_t x0, int16_t x1, const char *label,
                             float value, float target, const char *unit)
{
    const int16_t y0 = 162;
    const int16_t y1 = 234;
    char line[24];

    eve.COLOR_RGB(22, 32, 27);
    draw_round_rect(eve, x0, y0, x1, y1, 8);

    // Accent strip
    eve.COLOR_RGB(0, 230, 118);
    draw_round_rect(eve, x0 + 6, y0 + 12, x0 + 10, y1 - 12, 2);

    eve.COLOR_RGB(140, 160, 150);
    eve.CMD_TEXT(x0 + 20, y0 + 8, 21, 0, label);

    format_tenths(line, value, unit);
    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(x0 + 20, y0 + 30, 30, 0, line);
}

static void draw_charging_overlay(Bridgetek_EVE2 &eve, const dashboard_data_t *d)
{
    char line[24];
    uint8_t pct = charge_percent(d);

    eve.SAVE_CONTEXT();

    eve.TAG(255);

    // Background
    eve.CMD_GRADIENT(0, 0, 0x06201A, 0, 272, 0x000000);

    // Header: lightning bolt + title
    eve.COLOR_RGB(255, 214, 0);
    eve.LINE_WIDTH(3 * 16);
    eve.BEGIN(eve.BEGIN_LINE_STRIP);
    eve.VERTEX2F(160 * 16, 14 * 16);
    eve.VERTEX2F(150 * 16, 32 * 16);
    eve.VERTEX2F(162 * 16, 32 * 16);
    eve.VERTEX2F(152 * 16, 50 * 16);
    eve.END();

    eve.COLOR_RGB(0, 230, 118);
    eve.CMD_TEXT(250, 32, 30, Bridgetek_EVE2::OPT_CENTER, "CHARGING");

    // Battery body + terminal
    const int16_t bx0 = 70, by0 = 66, bx1 = 390, by1 = 146;
    eve.COLOR_RGB(200, 210, 205);
    draw_round_rect(eve, bx0, by0, bx1, by1, 10);
    draw_round_rect(eve, bx1 + 2, 90, bx1 + 14, 122, 4);
    eve.COLOR_RGB(8, 16, 12);
    draw_round_rect(eve, bx0 + 4, by0 + 4, bx1 - 4, by1 - 4, 7);

    // Fill, colored by charge level
    const int16_t fx0 = bx0 + 8, fy0 = by0 + 8, fx1 = bx1 - 8, fy1 = by1 - 8;
    int16_t fill_w = (int16_t)(((int32_t)(fx1 - fx0) * pct) / 100);
    if(fill_w >= 8)
    {
        if(pct < 20)      eve.COLOR_RGB(255, 60, 60);
        else if(pct < 50) eve.COLOR_RGB(255, 176, 0);
        else              eve.COLOR_RGB(0, 200, 100);
        draw_round_rect(eve, fx0, fy0, fx0 + fill_w, fy1, 4);

        // Moving shine inside the fill to show that energy is flowing
        const int16_t band = 36;
        int16_t pos = (int16_t)((s_charging_anim * 4U) % (uint32_t)(fill_w + band)) - band;
        eve.SCISSOR_XY(fx0, fy0);
        eve.SCISSOR_SIZE(fill_w, fy1 - fy0);
        eve.COLOR_RGB(255, 255, 255);
        eve.COLOR_A(55);
        eve.BEGIN(eve.BEGIN_RECTS);
        eve.LINE_WIDTH(16);
        eve.VERTEX2F((fx0 + pos) * 16, fy0 * 16);
        eve.VERTEX2F((fx0 + pos + band) * 16, fy1 * 16);
        eve.END();
        eve.COLOR_A(255);
        eve.SCISSOR_XY(0, 0);
        eve.SCISSOR_SIZE(480, 272);
    }

    // Percentage with a soft shadow so it stays readable over any fill color
    sprintf(line, "%u%%", (unsigned)pct);
    eve.COLOR_RGB(0, 0, 0);
    eve.COLOR_A(140);
    eve.CMD_TEXT(232, 108, 31, Bridgetek_EVE2::OPT_CENTER, line);
    eve.COLOR_A(255);
    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(230, 106, 31, Bridgetek_EVE2::OPT_CENTER, line);

    // Voltage / current tiles
    draw_charge_tile(eve, 24, 234, "VOLTAGE", d->charge_voltage, d->charge_cmd_voltage, "V");
    draw_charge_tile(eve, 246, 456, "CURRENT", d->charge_current, d->charge_cmd_current, "A");

    // Footer
    if(d->charger_flags != 0)
    {
        eve.COLOR_RGB(255, 176, 0);
        sprintf(line, "FLAGS 0x%02X", (unsigned)d->charger_flags);
        eve.CMD_TEXT(24, 254, 20, Bridgetek_EVE2::OPT_CENTERY, line);
    }
    eve.COLOR_RGB(110, 125, 118);
    eve.CMD_TEXT(240, 254, 20, Bridgetek_EVE2::OPT_CENTER, "TOUCH TO CLOSE");

    eve.RESTORE_CONTEXT();
}


/**************************************************************************************
 * Functions to initialize and update the dashboard data, and to draw the dashboard   
 * on the EVE display. These functions can be called from the main loop or from a timer  
 * interrupt to keep the dashboard updated with the latest data from the ECU.         
 * Make sure to call update_dashboard_data() after updating the main data fields 
 * to push the latest values into the graph buffers, and then call update_dashboard_draw() 
 * to refresh the display.
 **************************************************************************************
 */

void init_dashboard(dashboard_data_t *data)
{
    s_log_tick = 0;
    dashboard_logs_clear();
    dashboard_race_overlay_acknowledge();

    data->rpm = 0;

    data->tps = 0;
    data->tps_1 = 0;
    data->tps_2 = 0;

    data->motor1_ac_current = 0;
    data->motor1_temp = 0;
    data->motor1_rated_current = 0;

    data->motor2_ac_current = 0;
    data->motor2_temp = 0;
    data->motor2_rated_current = 0;

    data->brake_front = 0;
    data->brake_rear = 0;
    data->steering_angle = 0;

    data->battery_voltage = 0;
    data->battery_current = 0;

    data->wheel_speed_fl = 0;
    data->wheel_speed_fr = 0;
    data->wheel_speed_rl = 0;
    data->wheel_speed_rr = 0;

    data->driver1_temp = 0;
    data->driver1_dc_voltage = 0;
    data->driver1_dc_current = 0;

    data->driver2_temp = 0;
    data->driver2_dc_voltage = 0;
    data->driver2_dc_current = 0;

    data->driver1_warning = NO_WARNING;
    data->driver1_error = NO_FAULT;

    data->driver2_warning = NO_WARNING;
    data->driver2_error = NO_FAULT;

    
    // Buttons and mode
    data->canopen_state = 0;
    data->drive_enabled = 0;
    data->traction_on = 1;
    data->telemetry_enabled = 1;
    data->mode = 0;

    data->cal_tps_0 = 0;
    data->cal_tps_100 = 0;
    data->cal_left_steer = 0;
    data->cal_center_steer = 0;
    data->cal_right_steer = 0;
    data->cal_current_sensors = 0;

    data->cal_screen = 0;

    data->charging = 0;
    data->charger_flags = 0;
    data->charge_cmd_voltage = 0;
    data->charge_cmd_current = 0;
    data->charge_voltage = 0;
    data->charge_current = 0;
    s_charging_overlay = false;
    s_charging_snooze = 0;
    s_implausibility = 0;
    data->apps_brake_plausibility = 0;
    data->sensor_can_timeout = 0;

    

    // Initialize graph buffers to zero
    for(int i=0; i<GRAPH_BUFFER_SIZE; i++) {
        data->driver1_dc_voltage_history[i] = 0;
        data->driver1_dc_current_history[i] = 0;
        data->motor1_ac_current_history[i] = 0;
        data->motor1_temp_history[i] = 0;
        
        data->driver2_dc_voltage_history[i] = 0;
        data->driver2_dc_current_history[i] = 0;
        data->motor2_ac_current_history[i] = 0;
        data->motor2_temp_history[i] = 0;
        
        data->steering_angle_history[i] = 0;
        data->tps_history[i] = 0;
        data->brake_front_history[i] = 0;
        data->brake_rear_history[i] = 0;
        
        data->wheel_speed_fl_history[i] = 0;
        data->wheel_speed_fr_history[i] = 0;
        data->wheel_speed_rl_history[i] = 0;
        data->wheel_speed_rr_history[i] = 0;
    }
}

void update_dashboard_data(dashboard_data_t *data)
{
    static bool prev_valid = false;
    static uint8_t prev_drv1_warn = NO_WARNING;
    static uint8_t prev_drv1_err = NO_FAULT;
    static uint8_t prev_drv2_warn = NO_WARNING;
    static uint8_t prev_drv2_err = NO_FAULT;
    static uint8_t prev_can_state = BOOTUP;
    static uint8_t prev_charging = 0;

    s_log_tick++;

    // This function can be called after updating the main data fields to push the latest values into the graph buffers
    data->driver1_dc_voltage_history[data->graph_buffer_index] = data->driver1_dc_voltage;
    data->driver1_dc_current_history[data->graph_buffer_index] = data->driver1_dc_current;
    data->motor1_ac_current_history[data->graph_buffer_index] = data->motor1_ac_current;
    data->motor1_temp_history[data->graph_buffer_index] = data->motor1_temp;
    
    data->driver2_dc_voltage_history[data->graph_buffer_index] = data->driver2_dc_voltage;
    data->driver2_dc_current_history[data->graph_buffer_index] = data->driver2_dc_current;
    data->motor2_ac_current_history[data->graph_buffer_index] = data->motor2_ac_current;
    data->motor2_temp_history[data->graph_buffer_index] = data->motor2_temp;
    
    data->steering_angle_history[data->graph_buffer_index] = data->steering_angle;
    data->tps_history[data->graph_buffer_index] = data->tps;
    data->brake_front_history[data->graph_buffer_index] = data->brake_front;
    data->brake_rear_history[data->graph_buffer_index] = data->brake_rear;
    
    data->wheel_speed_fl_history[data->graph_buffer_index] = data->wheel_speed_fl;
    data->wheel_speed_fr_history[data->graph_buffer_index] = data->wheel_speed_fr;
    data->wheel_speed_rl_history[data->graph_buffer_index] = data->wheel_speed_rl;
    data->wheel_speed_rr_history[data->graph_buffer_index] = data->wheel_speed_rr;

    // Increment buffer index
    data->graph_buffer_index = (data->graph_buffer_index + 1) % GRAPH_BUFFER_SIZE;

    if(!prev_valid)
    {
        prev_drv1_warn = data->driver1_warning;
        prev_drv1_err = data->driver1_error;
        prev_drv2_warn = data->driver2_warning;
        prev_drv2_err = data->driver2_error;
        prev_can_state = data->canopen_state;
        prev_charging = data->charging;
        prev_valid = true;
        return;
    }

    char log_text[54];

    if(data->canopen_state != prev_can_state)
    {
        sprintf(log_text, "CAN %s -> %s", canopen_to_str(prev_can_state), canopen_to_str(data->canopen_state));
        dashboard_log_event(0, LOG_INFO, log_text);
        prev_can_state = data->canopen_state;
    }

    if(data->charging != prev_charging)
    {
        dashboard_log_event(0, LOG_INFO, data->charging ? "Charging started" : "Charging stopped");
        prev_charging = data->charging;
    }

    if(data->driver1_warning != prev_drv1_warn)
    {
        if(data->driver1_warning == NO_WARNING)
        {
            sprintf(log_text, "Warning cleared (%s)", warning_to_str(prev_drv1_warn));
            dashboard_log_event(1, LOG_INFO, log_text);
        }
        else
        {
            sprintf(log_text, "Warning %s", warning_to_str(data->driver1_warning));
            dashboard_log_event(1, LOG_WARNING, log_text);
        }
        prev_drv1_warn = data->driver1_warning;
    }

    if(data->driver2_warning != prev_drv2_warn)
    {
        if(data->driver2_warning == NO_WARNING)
        {
            sprintf(log_text, "Warning cleared (%s)", warning_to_str(prev_drv2_warn));
            dashboard_log_event(2, LOG_INFO, log_text);
        }
        else
        {
            sprintf(log_text, "Warning %s", warning_to_str(data->driver2_warning));
            dashboard_log_event(2, LOG_WARNING, log_text);
        }
        prev_drv2_warn = data->driver2_warning;
    }

    if(data->driver1_error != prev_drv1_err)
    {
        if(data->driver1_error == NO_FAULT)
        {
            sprintf(log_text, "Fault cleared (%s)", error_to_str(prev_drv1_err));
            dashboard_log_event(1, LOG_INFO, log_text);
        }
        else
        {
            sprintf(log_text, "Fault %s", error_to_str(data->driver1_error));
            dashboard_log_event(1, LOG_FAULT, log_text);
        }
        prev_drv1_err = data->driver1_error;
    }

    if(data->driver2_error != prev_drv2_err)
    {
        if(data->driver2_error == NO_FAULT)
        {
            sprintf(log_text, "Fault cleared (%s)", error_to_str(prev_drv2_err));
            dashboard_log_event(2, LOG_INFO, log_text);
        }
        else
        {
            sprintf(log_text, "Fault %s", error_to_str(data->driver2_error));
            dashboard_log_event(2, LOG_FAULT, log_text);
        }
        prev_drv2_err = data->driver2_error;
    }
}

static void draw_navigation_bar(Bridgetek_EVE2 &eve)
{
    // Flecha izquierda (tag 90): area tactil invisible ampliada + flecha visible
    eve.TAG(90);
    eve.COLOR_MASK(0, 0, 0, 0);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F(0 * 16, 90 * 16);
    eve.VERTEX2F(70 * 16, 182 * 16);
    eve.END();
    eve.COLOR_MASK(1, 1, 1, 1);

    eve.LINE_WIDTH(2 * 16);
    eve.COLOR_RGB(140,140,140);
    eve.BEGIN(eve.BEGIN_LINES);
    eve.VERTEX2F(20 * 16, 136 * 16);
    eve.VERTEX2F(13 * 16, 129 * 16);
    eve.VERTEX2F(20 * 16, 136 * 16);
    eve.VERTEX2F(13 * 16, 143 * 16);
    eve.END();

    // Flecha derecha (tag 91): area tactil invisible ampliada + flecha visible
    eve.TAG(91);
    eve.COLOR_MASK(0, 0, 0, 0);
    eve.BEGIN(eve.BEGIN_RECTS);
    eve.VERTEX2F(410 * 16, 90 * 16);
    eve.VERTEX2F(480 * 16, 182 * 16);
    eve.END();
    eve.COLOR_MASK(1, 1, 1, 1);

    eve.LINE_WIDTH(2 * 16);
    eve.BEGIN(eve.BEGIN_LINES);
    eve.VERTEX2F(460 * 16, 136 * 16);
    eve.VERTEX2F(467 * 16, 129 * 16);
    eve.VERTEX2F(460 * 16, 136 * 16);
    eve.VERTEX2F(467 * 16, 143 * 16);
    eve.END();

    eve.TAG(255);
}

static void draw_wheel_rpm_diff_alert(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    float front_avg = (d->wheel_speed_fl + d->wheel_speed_fr) * 0.5f;
    float rear_avg = (d->wheel_speed_rl + d->wheel_speed_rr) * 0.5f;

    if(front_avg < 0.0f) front_avg = -front_avg;
    if(rear_avg < 0.0f) rear_avg = -rear_avg;

    float ref = (front_avg > rear_avg) ? front_avg : rear_avg;
    if(ref < 1.0f)
    {
        return;
    }

    float diff_ratio = fabsf(rear_avg - front_avg) / ref;
    if(diff_ratio < 0.20f)
    {
        return;
    }

    // Indicador chico en esquina superior izquierda.
    eve.COLOR_RGB(255,0,0);
    eve.POINT_SIZE(10 * 16);
    eve.BEGIN(eve.BEGIN_POINTS);
    eve.VERTEX2F(14 * 16, 14 * 16);
    eve.END();
}

void update_dashboard_draw(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    eve.LIB_BeginCoProList();

    eve.CMD_DLSTART();

    eve.CLEAR_COLOR_RGB(0,0,0);
    eve.CLEAR(1,1,1);

    if(current_page == PAGE_RACE)
    {
        draw_race_core(eve, d);
        draw_wheel_rpm_diff_alert(eve,d);

        uint8_t fault_driver = 0;
        uint8_t fault_code = 0;
        uint8_t warn_driver = 0;
        uint8_t warn_code = 0;
        bool has_fault = race_find_fault(d, &fault_driver, &fault_code);
        bool has_warning = race_find_warning(d, &warn_driver, &warn_code);

        if(!has_fault)
        {
            s_fault_acked = false;
        }
        if(!has_warning)
        {
            s_warn_acked = false;
        }

        if(has_fault && !(s_fault_acked && s_acked_fault_driver == fault_driver && s_acked_fault_code == fault_code))
        {
            s_race_overlay = RACE_OVERLAY_FAULT;
            s_overlay_driver = fault_driver;
            s_overlay_code = fault_code;
        }
        else if(has_warning && !(s_warn_acked && s_acked_warn_driver == warn_driver && s_acked_warn_code == warn_code))
        {
            s_race_overlay = RACE_OVERLAY_WARNING;
            s_overlay_driver = warn_driver;
            s_overlay_code = warn_code;
        }
        else if(!has_fault && !has_warning)
        {
            dashboard_race_overlay_acknowledge();
        }
        else
        {
            s_race_overlay = RACE_OVERLAY_NONE;
        }

        if(s_race_overlay == RACE_OVERLAY_FAULT)
        {
            draw_fault_overlay(eve, s_overlay_driver, s_overlay_code);
        }
        else if(s_race_overlay == RACE_OVERLAY_WARNING)
        {
            draw_warning_overlay(eve, s_overlay_driver, s_overlay_code);
        }
    }

    if(current_page == PAGE_NORMAL)
    {
        draw_race_classic(eve, d);
        draw_wheel_rpm_diff_alert(eve,d);
    }

    if(current_page == PAGE_TELEMETRY)
    {
        draw_telemetry_grid(eve, d);
    }

    if(current_page == PAGE_GRAPH)
    {
        // título del gráfico
        const char* graphTitle = "GRAPH";
        switch(current_graph)
        {
            case GRAPH_DRV1_VDC: graphTitle = "Driver 1 Voltage"; break;
            case GRAPH_DRV1_IDC: graphTitle = "Driver 1 Current"; break;
            case GRAPH_M1_IAC: graphTitle = "Motor 1 AC Current"; break;
            case GRAPH_M1_T: graphTitle = "Motor 1 Temp"; break;

            case GRAPH_DRV2_VDC: graphTitle = "Driver 2 Voltage"; break;
            case GRAPH_DRV2_IDC: graphTitle = "Driver 2 Current"; break;
            case GRAPH_M2_IAC: graphTitle = "Motor 2 AC Current"; break;
            case GRAPH_M2_T: graphTitle = "Motor 2 Temp"; break;

            case GRAPH_TPS: graphTitle = "TPS"; break;
            case GRAPH_STEER: graphTitle = "Steering Angle"; break;
            case GRAPH_FRONT_BRK: graphTitle = "Front Brake"; break;
            case GRAPH_REAR_BRK: graphTitle = "Rear Brake"; break;

            case GRAPH_FL_SPD: graphTitle = "FL Wheel Speed"; break;
            case GRAPH_FR_SPD: graphTitle = "FR Wheel Speed"; break;
            case GRAPH_RL_SPD: graphTitle = "RL Wheel Speed"; break;
            case GRAPH_RR_SPD: graphTitle = "RR Wheel Speed"; break;

            default: graphTitle = "Unknown"; break;
        }
        eve.CMD_TEXT(240, 20, 26, Bridgetek_EVE2::OPT_CENTER, graphTitle);

        // área del gráfico
        int graphX = 30;
        int graphY = 35;
        int graphW = 420;
        int graphH = 180;

        // fondo del gráfico
        eve.COLOR_RGB(20,20,20);
        eve.BEGIN(eve.BEGIN_RECTS);
        eve.VERTEX2F(graphX*16, graphY*16);
        eve.VERTEX2F((graphX+graphW)*16, (graphY+graphH)*16);
        eve.END();

        // grid
        eve.COLOR_RGB(50,50,50);
        eve.BEGIN(eve.BEGIN_LINES);
        for(int i=0; i<=10; i++)
        {
            int x = graphX + i * (graphW/10);
            eve.VERTEX2F(x*16, graphY*16);
            eve.VERTEX2F(x*16, (graphY+graphH)*16);
        }
        for(int i=0; i<=5; i++)
        {
            int y = graphY + i * (graphH/5);
            eve.VERTEX2F(graphX*16, y*16);
            eve.VERTEX2F((graphX+graphW)*16, y*16);
        }
        eve.END();

        // l�nea del gr�fico (hist�rica)
        eve.COLOR_RGB(0,255,0);
        eve.BEGIN(eve.BEGIN_LINE_STRIP);

        float max_val = 100; // default
        float *history = nullptr;

        switch(current_graph)
        {
            case GRAPH_DRV1_VDC: max_val = 200; history = d->driver1_dc_voltage_history; break;
            case GRAPH_DRV1_IDC: max_val = 200; history = d->driver1_dc_current_history; break;
            case GRAPH_M1_IAC: max_val = 400; history = d->motor1_ac_current_history; break;
            case GRAPH_M1_T: max_val = 80; history = d->motor1_temp_history; break;

            case GRAPH_DRV2_VDC: max_val = 200; history = d->driver2_dc_voltage_history; break;
            case GRAPH_DRV2_IDC: max_val = 200; history = d->driver2_dc_current_history; break;
            case GRAPH_M2_IAC: max_val = 400; history = d->motor2_ac_current_history; break;
            case GRAPH_M2_T: max_val = 80; history = d->motor2_temp_history; break;

            case GRAPH_TPS: max_val = 100; history = d->tps_history; break;
            case GRAPH_STEER: max_val = 360; history = d->steering_angle_history; break;
            case GRAPH_FRONT_BRK: max_val = 100; history = d->brake_front_history; break;
            case GRAPH_REAR_BRK: max_val = 100; history = d->brake_rear_history; break;

            case GRAPH_FL_SPD: max_val = 3000; history = d->wheel_speed_fl_history; break;
            case GRAPH_FR_SPD: max_val = 3000; history = d->wheel_speed_fr_history; break;
            case GRAPH_RL_SPD: max_val = 3000; history = d->wheel_speed_rl_history; break;
            case GRAPH_RR_SPD: max_val = 3000; history = d->wheel_speed_rr_history; break;
            default: history = nullptr; break;
        }

        if(history)
        {
            for(int i = 0; i < GRAPH_BUFFER_SIZE; i++)
            {
                int idx = (d->graph_buffer_index + i) % GRAPH_BUFFER_SIZE;

                float val = history[idx];

                if(current_graph == GRAPH_STEER)
                    val = fabs(val);

                // limitar valores
                if(val < 0) val = 0;
                if(val > max_val) val = max_val;

                // FIX divisi�n entera
                int x = graphX + (i * graphW) / (GRAPH_BUFFER_SIZE - 1);

                float norm = val / max_val;

                int y = graphY + graphH - (int)(norm * graphH);

                eve.VERTEX2F(x * 16, y * 16);
            }
        }

        eve.END();

        // mostrar valor actual
        float current_val = 0;
        switch(current_graph)
        {
            case GRAPH_DRV1_VDC: current_val = d->driver1_dc_voltage; break;
            case GRAPH_DRV1_IDC: current_val = d->driver1_dc_current; break;
            case GRAPH_M1_IAC: current_val = d->motor1_ac_current; break;
            case GRAPH_M1_T: current_val = d->motor1_temp; break;

            case GRAPH_DRV2_VDC: current_val = d->driver2_dc_voltage; break;
            case GRAPH_DRV2_IDC: current_val = d->driver2_dc_current; break;
            case GRAPH_M2_IAC: current_val = d->motor2_ac_current; break;
            case GRAPH_M2_T: current_val = d->motor2_temp; break;

            case GRAPH_TPS: current_val = d->tps; break;
            case GRAPH_STEER: current_val = abs(d->steering_angle); break;
            case GRAPH_FRONT_BRK: current_val = d->brake_front; break;
            case GRAPH_REAR_BRK: current_val = d->brake_rear; break;

            case GRAPH_FL_SPD: current_val = d->wheel_speed_fl; break;
            case GRAPH_FR_SPD: current_val = d->wheel_speed_fr; break;
            case GRAPH_RL_SPD: current_val = d->wheel_speed_rl; break;
            case GRAPH_RR_SPD: current_val = d->wheel_speed_rr; break;

            default: current_val = 0; break;
        }
        int y_val = graphY + graphH - (current_val / max_val * graphH);
        char val_str[16];
        dtostrf(current_val, 4, 1, val_str);
        eve.CMD_TEXT(graphX + graphW + 10, y_val, 26, 0, val_str);

        // escala Y
        eve.COLOR_RGB(255,255,255);
        char scale_str[16];
        dtostrf(max_val, 4, 0, scale_str);
        eve.CMD_TEXT(graphX - 35, graphY, 22, 0, scale_str);
        dtostrf(max_val/2, 4, 0, scale_str);
        eve.CMD_TEXT(graphX - 35, graphY + graphH/2, 22, 0, scale_str);
        eve.CMD_TEXT(graphX - 10, graphY + graphH, 22, 0, "0");

        // botón back
        eve.TAG(30);
        eve.COLOR_RGB(100,100,100);
        eve.CMD_BUTTON(200, 235, 80, 30, 26, 0, "BACK");
        eve.TAG(255);
    }

    if(current_page == PAGE_DEBUG)
    {
        // título
        eve.CMD_TEXT(240, 20, 22, Bridgetek_EVE2::OPT_CENTER, "DEBUG MENU");


        

// fila 1: calibraciones TPS
        eve.TAG(40);

        if(d->cal_tps_0)
        {
            eve.COLOR_RGB(0,0,255);
            eve.CMD_BUTTON(20, 40, 120, 40, 26, EVE_OPT_FLAT, "Send");

            if(cal_tps_0_timer > 0)
            {
                cal_tps_0_timer--;
            }
            else
            {
                d->cal_tps_0 = 0;
            }
        }
        else
        {
            eve.COLOR_RGB(255,255,255);
            eve.CMD_BUTTON(20, 40, 120, 40, 26, 0, "Cal TPS 0%");
        }

        eve.TAG(41);
        if(d->cal_tps_100)
        {
            eve.COLOR_RGB(0,0,255);
            eve.CMD_BUTTON(170, 40, 120, 40, 26, EVE_OPT_FLAT, "Send");

            if(cal_tps_100_timer > 0)
            {
                cal_tps_100_timer--;
            }
            else
            {
                d->cal_tps_100 = 0;
            }
        }
        else
        {
            eve.COLOR_RGB(255,255,255);
            eve.CMD_BUTTON(170, 40, 120, 40, 26, 0, "Cal TPS 100%");
        }
        
        
        // fila 2: modos
        if (d->traction_on)
        {
            eve.TAG(42);
            eve.COLOR_RGB(0,255,0);
            eve.CMD_BUTTON(320, 40, 120, 40, 26, EVE_OPT_FLAT, "Traction: ON");
        }
        else
        {
            eve.TAG(42);
            eve.COLOR_RGB(255,0,0);
            eve.CMD_BUTTON(320, 40, 120, 40, 26, 0, "Traction: OFF");
        }

        if(d->mode == RACE)
        {
            eve.TAG(43);
            eve.COLOR_RGB(255,0,0);
            eve.CMD_BUTTON(20, 100, 120, 40, 26, EVE_OPT_FLAT, "Mode: RACE");
        }else
        {             
            eve.TAG(43);
            eve.COLOR_RGB(0,255,0);
            eve.CMD_BUTTON(20, 100, 120, 40, 26, 0, "Mode: NORMAL");
        }
        
        if(d->drive_enabled)
        {
            eve.TAG(44);
            eve.COLOR_RGB(0,255,0);
            eve.CMD_BUTTON(170, 100, 120, 40, 26, EVE_OPT_FLAT, "Drive: ON");
        }else
        {
            eve.TAG(44);
            eve.COLOR_RGB(255,0,0);
            eve.CMD_BUTTON(170, 100, 120, 40, 26, 0, "Drive: OFF");
        }

        if(d->telemetry_enabled)
        {
            eve.TAG(45);
            eve.COLOR_RGB(0,255,0);
            eve.CMD_BUTTON(320, 100, 120, 40, 26, EVE_OPT_FLAT, "Telemetry: ON");
        }else
        {
            eve.TAG(45);
            eve.COLOR_RGB(255,0,0);
            eve.CMD_BUTTON(320, 100, 120, 40, 26, 0, "Telemetry: OFF");
        }

        // fila 3: steering
        eve.TAG(46);
        if (d->cal_left_steer)
        {
            eve.COLOR_RGB(0,0,255);
            eve.CMD_BUTTON(20, 160, 120, 40, 26, EVE_OPT_FLAT, "Send");
            if(cal_left_steer_timer > 0)
            {
                cal_left_steer_timer--;
            }
            else
            {
                d->cal_left_steer = 0;
            }
        }else
        {
            eve.COLOR_RGB(255,255,255);
            eve.CMD_BUTTON(20, 160, 120, 40, 26, 0, "Cal LEFT");
        }


        eve.TAG(47);
        if(d->cal_right_steer)
        {
            eve.COLOR_RGB(0,0,255);
            eve.CMD_BUTTON(170, 160, 120, 40, 26, EVE_OPT_FLAT, "Send");
            if(cal_right_steer_timer > 0)
            {
                cal_right_steer_timer--;
            }
            else
            {
                d->cal_right_steer = 0;
            }
        }else
        {
            eve.COLOR_RGB(255,255,255);
            eve.CMD_BUTTON(170, 160, 120, 40, 26, 0, "Cal RIGHT");
        }
        

        eve.TAG(48);
        if(d->cal_center_steer)
        {
            eve.COLOR_RGB(0,0,255);
            eve.CMD_BUTTON(320, 160, 120, 40, 26, EVE_OPT_FLAT, "Send");
            if(cal_center_steer_timer > 0)
            {
                cal_center_steer_timer--;
            }
            else
            {
                d->cal_center_steer = 0;
            }
        }
        else
        {
            eve.COLOR_RGB(255,255,255);
            eve.CMD_BUTTON(320, 160, 120, 40, 26, 0, "Cal CENTER");
        }



        // fila 4: calibrate screen
        if(d->cal_screen)
        {
            eve.TAG(49);
            eve.COLOR_RGB(0,0,255);
            eve.CMD_BUTTON(20, 220, 120, 40, 26, EVE_OPT_FLAT, "Calibrating...");
        }else
        {
            eve.TAG(49);
            eve.COLOR_RGB(255,255,255);
            eve.CMD_BUTTON(20, 220, 120, 40, 26, 0, "Cal Screen");
        }

        eve.TAG(50);
        if(d->cal_current_sensors)
        {
            eve.COLOR_RGB(0,0,255);
            eve.CMD_BUTTON(170, 220, 120, 40, 26, EVE_OPT_FLAT, "Send");
            if(cal_current_sensors_timer > 0)
            {
                cal_current_sensors_timer--;
            }
            else
            {
                d->cal_current_sensors = 0;
            }
        }
        else
        {
            eve.COLOR_RGB(255,255,255);
            eve.CMD_BUTTON(170, 220, 120, 40, 26, 0, "Cal Current");
        }

        // eve.TAG(51);
        // eve.CMD_BUTTON(320, 220, 120, 40, 26, 0, "");

        eve.TAG(52);
        if(test_data_is_enabled())
        {
            eve.COLOR_RGB(0,140,255);
            eve.CMD_BUTTON(320, 220, 120, 40, 26, EVE_OPT_FLAT, "TEST DATA: ON");
        }
        else
        {
            eve.COLOR_RGB(255,255,255);
            eve.CMD_BUTTON(320, 220, 120, 40, 26, 0, "TEST DATA: OFF");
        }

        eve.TAG(255);
    }

    if(current_page == PAGE_FAULTS)
    {
        eve.CMD_TEXT(240, 10, 22, Bridgetek_EVE2::OPT_CENTER, "FAULTS / CAN LOG");

        const int log_x = 18;
        const int log_y = 34;
        const int log_w = 400;
        const int log_h = 210;
        const int line_h = 22;

        eve.COLOR_RGB(20,20,20);
        eve.BEGIN(eve.BEGIN_RECTS);
        eve.VERTEX2F(log_x * 16, log_y * 16);
        eve.VERTEX2F((log_x + log_w) * 16, (log_y + log_h) * 16);
        eve.END();

        uint16_t start = 0;
        if(s_log_count > DASHBOARD_LOG_VISIBLE_LINES)
        {
            uint16_t max_start = (uint16_t)(s_log_count - DASHBOARD_LOG_VISIBLE_LINES);
            if(s_log_scroll > max_start)
            {
                s_log_scroll = max_start;
            }
            start = (uint16_t)(max_start - s_log_scroll);
        }

        dashboard_log_entry_t entry;
        char ts[20];
        char line[96];

        for(uint16_t i = 0; i < DASHBOARD_LOG_VISIBLE_LINES; i++)
        {
            uint16_t idx = (uint16_t)(start + i);
            if(!log_get_by_order(idx, &entry))
            {
                break;
            }

            if(entry.level == LOG_WARNING)
            {
                eve.COLOR_RGB(255,255,0);
            }
            else if(entry.level == LOG_FAULT)
            {
                eve.COLOR_RGB(255,0,0);
            }
            else
            {
                eve.COLOR_RGB(220,220,220);
            }

            const char *driver_txt = "SYS";
            if(entry.driver == 1) driver_txt = "DRV1";
            if(entry.driver == 2) driver_txt = "DRV2";

            format_log_timestamp(entry.tick, ts);
            sprintf(line, "%s %s  %s", ts, driver_txt, entry.message);
            eve.CMD_TEXT(log_x + 6, log_y + 6 + (i * line_h), 21, 0, line);
        }

        // Scroll up
        eve.TAG(70);
        eve.COLOR_RGB(160,160,160);
        eve.LINE_WIDTH(2 * 16);
        eve.BEGIN(eve.BEGIN_LINES);
        eve.VERTEX2F(442 * 16, 70 * 16);
        eve.VERTEX2F(450 * 16, 62 * 16);
        eve.VERTEX2F(458 * 16, 70 * 16);
        eve.VERTEX2F(450 * 16, 62 * 16);
        eve.END();

        // Scroll down
        eve.TAG(71);
        eve.BEGIN(eve.BEGIN_LINES);
        eve.VERTEX2F(442 * 16, 200 * 16);
        eve.VERTEX2F(450 * 16, 208 * 16);
        eve.VERTEX2F(458 * 16, 200 * 16);
        eve.VERTEX2F(450 * 16, 208 * 16);
        eve.END();

        // Clear logs
        eve.TAG(72);
        eve.COLOR_RGB(100,100,100);
        eve.CMD_BUTTON(425, 230, 50, 28, 21, 0, "CLR");
        eve.TAG(255);
    }

    // Charging overlay (any page). A race warning/fault overlay has priority.
    if(d->charging)
    {
        if(s_charging_snooze > 0)
        {
            s_charging_snooze--;
        }
        else
        {
            s_charging_overlay = true;
        }
    }
    else
    {
        s_charging_overlay = false;
        s_charging_snooze = 0;
    }

    bool race_overlay = (current_page == PAGE_RACE && dashboard_race_overlay_is_active());
    bool charging_overlay = s_charging_overlay && !race_overlay;
    if(charging_overlay)
    {
        s_charging_anim++;
        draw_charging_overlay(eve, d);
    }

    // Implausibility alert (any page), below the brake alert so a blocked re-enable is still visible.
    if(s_implausibility)
    {
        draw_implausibility_overlay(eve);
    }

    // APPS/brake plausibility (any page), above the implausibility alert: it needs immediate action.
    if(d->apps_brake_plausibility)
    {
        draw_apps_brake_plausibility_overlay(eve);
    }

    // Sensor CAN timeout (any page): drive is off and cannot be enabled while it lasts.
    if(d->sensor_can_timeout)
    {
        draw_sensor_can_timeout_overlay(eve, d->sensor_can_timeout);
    }

    // Brake alert (any page) is drawn on top of everything; it closes itself once the brake is pressed.
    if(s_brake_alert && d->brake_front > DRIVE_ENABLE_MIN_BRAKE_PSI)
    {
        dashboard_brake_alert_acknowledge();
    }
    if(s_brake_alert)
    {
        draw_brake_alert_overlay(eve, d);
    }

    if(!race_overlay && !charging_overlay && !s_implausibility && !s_brake_alert && !d->apps_brake_plausibility && !d->sensor_can_timeout)
    {
        draw_navigation_bar(eve);
    }

    eve.DISPLAY();

    eve.CMD_SWAP();

    eve.LIB_EndCoProList();
}
