#include "dashboard.h"
#include "ui_rpm_bar.h"
#include "ui_buttons.h"
#include "pages.h"
#include "ui_touch.h"
#include <stdio.h>

#include <math.h>
#include <stdio.h>


dashboard_data_t dashboard_data;

int cal_tps_0_timer = 0;
int cal_tps_100_timer = 0;
int cal_left_steer_timer = 0;
int cal_right_steer_timer = 0;



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
    eve.CMD_PROGRESS(x,150,100,15,0,d->brake_front,100);

    eve.CMD_TEXT(x,180,26,0,"REAR BREAK");
    eve.CMD_NUMBER(x+100,180,26,0,d->brake_rear);
    eve.CMD_PROGRESS(x,200,100,15,0,d->brake_rear,100);
}

void draw_status(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    int x = 265;
    int y = 8;
    char line[64];

    auto warning_to_str = [](uint8_t warning) -> const char *
    {
        switch(warning)
        {
            case NO_WARNING: return "NO_WARNING";
            case CONTROLLER_TEMPERATURE_EXCEEDED: return "CTRL_TEMP";
            case MOTOR_TEMPERATURE_EXCEEDED: return "MOTOR_TEMP";
            case DC_LINK_UNDERVOLTAGE: return "DC_UV";
            case DC_LINK_OVERVOLTAGE: return "DC_OV";
            case STALL_PROTECTION: return "STALL";
            case MAX_VELOCITY_EXCEEDED: return "MAX_VEL";
            case BMS_PROPOSED_POWER: return "BMS_LIMIT";
            default: return "WARN_UNKNOWN";
        }
    };

    auto error_to_str = [](uint8_t error) -> const char *
    {
        if(error == (uint8_t)NO_FAULT) return "NO_FAULT";

        switch((uint16_t)(0xFF00U | error))
        {
            case ERROR_CURRENT_A: return "CURRENT_A";
            case ERROR_CURRENT_B: return "CURRENT_B";
            case ERROR_HS_FET: return "HS_FET";
            case ERROR_LS_FET: return "LS_FET";
            case ERROR_DRV_LS_L1: return "DRV_LS_L1";
            case ERROR_DRV_LS_L2: return "DRV_LS_L2";
            case ERROR_DRV_LS_L3: return "DRV_LS_L3";
            case ERROR_DRV_HS_L1: return "DRV_HS_L1";
            case ERROR_DRV_HS_L2: return "DRV_HS_L2";
            case ERROR_DRV_HS_L3: return "DRV_HS_L3";
            case ERROR_MOTOR_FEEDBACK: return "MOTOR_FB";
            case ERROR_DC_LINK_UNDERVOLTAGE: return "DC_UV";
            case ERROR_PULS_MODE_FINISHED: return "PULSE_END";
            case ERROR_APP_ERROR: return "APP_ERR";
            case ERROR_STO_ERROR: return "STO_ERR";
            case ERROR_CONTROLLER_OVERTEMPERATURE: return "CTRL_TEMP";
            default: return "ERR_UNKNOWN";
        }
    };

    auto drive_to_str = [](uint8_t drive_state) -> const char *
    {
        switch(drive_state)
        {
            case OFF: return "OFF";
            case ON: return "ON";
            case DRIVE_FAULT: return "FAULT";
            default: return "STATE_UNKNOWN";
        }
    };

    eve.COLOR_RGB(0,255,255);
    sprintf(line, "D1 W:%u %s", d->driver1_warning, warning_to_str(d->driver1_warning));
    eve.CMD_TEXT(x, y, 20, 0, line);

    eve.COLOR_RGB(255,120,120);
    sprintf(line, "D1 E:%u %s", d->driver1_error, error_to_str(d->driver1_error));
    eve.CMD_TEXT(x, y + 18, 20, 0, line);

    eve.COLOR_RGB(0,255,255);
    sprintf(line, "D2 W:%u %s", d->driver2_warning, warning_to_str(d->driver2_warning));
    eve.CMD_TEXT(x, y + 36, 20, 0, line);

    eve.COLOR_RGB(255,120,120);
    sprintf(line, "D2 E:%u %s", d->driver2_error, error_to_str(d->driver2_error));
    eve.CMD_TEXT(x, y + 54, 20, 0, line);

    eve.COLOR_RGB(255,255,0);
    sprintf(line, "DRV:%u %s", d->drive_state, drive_to_str(d->drive_state));
    eve.CMD_TEXT(x, y + 72, 20, 0, line);

    eve.COLOR_RGB(255,255,255);
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
    data->rpm = 0;

    data->tps = 0;
    data->tps_1 = 0;
    data->tps_2 = 0;

    data->motor1_ac_current = 0;
    data->motor1_dc_current = 0;
    data->motor1_dc_voltage = 0;
    data->motor1_temp = 0;

    data->motor2_ac_current = 0;
    data->motor2_dc_current = 0;
    data->motor2_dc_voltage = 0;
    data->motor2_temp = 0;

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
    data->driver1_voltage = 0;

    data->driver2_temp = 0;
    data->driver2_voltage = 0;

    data->driver1_warning = 0;
    data->driver1_error = 0;

    data->driver2_warning = 0;
    data->driver2_error = 0;

    
    // Buttons and mode
    data->drive_state = 0;
    data->drive_enabled = 0;
    data->traction_on = 0;
    data->telemetry_enabled = 0;
    data->mode = 0;

    data->cal_tps_0 = 0;
    data->cal_tps_100 = 0;
    data->cal_left_steer = 0;
    data->cal_right_steer = 0;

    data->cal_screen = 0;

    

    // Initialize graph buffers to zero
    for(int i=0; i<GRAPH_BUFFER_SIZE; i++) {
        data->motor1_dc_voltage_history[i] = 0;
        data->motor1_dc_current_history[i] = 0;
        data->motor1_ac_current_history[i] = 0;
        data->motor1_temp_history[i] = 0;
        
        data->motor2_dc_voltage_history[i] = 0;
        data->motor2_dc_current_history[i] = 0;
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
    // This function can be called after updating the main data fields to push the latest values into the graph buffers
    data->motor1_dc_voltage_history[data->graph_buffer_index] = data->motor1_dc_voltage;
    data->motor1_dc_current_history[data->graph_buffer_index] = data->motor1_dc_current;
    data->motor1_ac_current_history[data->graph_buffer_index] = data->motor1_ac_current;
    data->motor1_temp_history[data->graph_buffer_index] = data->motor1_temp;
    
    data->motor2_dc_voltage_history[data->graph_buffer_index] = data->motor2_dc_voltage;
    data->motor2_dc_current_history[data->graph_buffer_index] = data->motor2_dc_current;
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
}

void update_dashboard_draw(Bridgetek_EVE2 &eve, dashboard_data_t *d)
{
    eve.LIB_BeginCoProList();

    eve.CMD_DLSTART();

    eve.CLEAR_COLOR_RGB(0,0,0);
    eve.CLEAR(1,1,1);

    if(current_page == PAGE_RACE)
    {
        ui_draw_rpm_bar(eve,d->rpm);

        draw_motor_block(
            eve,
            20,
            "MOTOR 1",
            d->motor1_dc_voltage,
            d->motor1_dc_current,
            d->motor1_ac_current,
            d->motor1_temp
        );

        draw_motor_block(
            eve,
            360,
            "MOTOR 2",
            d->motor2_dc_voltage,
            d->motor2_dc_current,
            d->motor2_ac_current,
            d->motor2_temp
        );

        draw_center(eve,d);

        ui_draw_buttons(eve,d);

        draw_status(eve,d);
    }

    if(current_page == PAGE_TELEMETRY)
    {
        // título
        // dibujar rejilla 4x4
        const int cellW = 115;
        const int cellH = 60;
        int startX = 10;
        int startY = 10;
        eve.COLOR_RGB(255,255,255);
        // helper que maneja negativos mejor que CMD_NUMBER
        auto textInCell = [&](int col,int row,const char* label,float value,const char* unit="", int tag=0){
            int x = startX + col * (cellW + 10);
            int y = startY + row * (cellH + 10);
            eve.TAG(tag);
            eve.BEGIN(eve.BEGIN_RECTS);
            eve.COLOR_RGB(25,25,25);
            eve.VERTEX2F(x*16, y*16);
            eve.VERTEX2F((x+cellW)*16, (y+cellH)*16);
            eve.END();
            eve.COLOR_RGB(255,255,255);
            eve.CMD_TEXT(x+5, y+18, 22, 0, label);
            // convertir a entero con signo
            int iv = (int) value;
            char buf[16];
            sprintf(buf, "%d", iv);
            eve.CMD_TEXT(x+cellW/2, y+18, 22, 0, buf);
            if (unit[0]) eve.CMD_TEXT(x+cellW-20, y+18, 22, 0, unit);
            eve.TAG(255);
        };
        // columna 0: motor1 + battery volt
        textInCell(0,0,"M1 VDC", d->motor1_dc_voltage, "V", 10);
        textInCell(0,1,"M1 IDC", d->motor1_dc_current, "A", 11);
        textInCell(0,2,"M1 IAC", d->motor1_ac_current, "A", 12);
        textInCell(0,3,"M1 T", d->motor1_temp, "C", 13);
        // columna 1: motor2 + battery current
        textInCell(1,0,"M2 VDC", d->motor2_dc_voltage, "V", 14);
        textInCell(1,1,"M2 IDC", d->motor2_dc_current, "A", 15);
        textInCell(1,2,"M2 IAC", d->motor2_ac_current, "A", 16);
        textInCell(1,3,"M2 T", d->motor2_temp, "C", 17);
        // columna 2: controles
        textInCell(2,0,"TPS", d->tps, "%", 18);
        textInCell(2,1,"Steer", d->steering_angle, "deg", 19);
        textInCell(2,2,"F Brk", d->brake_front, "%", 20);
        textInCell(2,3,"R Brk", d->brake_rear, "%", 21);
        // columna 3: wheel speeds
        textInCell(3,0,"FL spd", d->wheel_speed_fl, "kmh", 22);
        textInCell(3,1,"FR spd", d->wheel_speed_fr, "kmh", 23);
        textInCell(3,2,"RL spd", d->wheel_speed_rl, "kmh", 24);
        textInCell(3,3,"RR spd", d->wheel_speed_rr, "kmh", 25);
    }

    if(current_page == PAGE_GRAPH)
    {
        // título del gráfico
        const char* graphTitle = "GRAPH";
        switch(current_graph)
        {
            case GRAPH_M1_VDC: graphTitle = "Motor 1 Voltage"; break;
            case GRAPH_M1_IDC: graphTitle = "Motor 1 Current"; break;
            case GRAPH_M1_IAC: graphTitle = "Motor 1 AC Current"; break;
            case GRAPH_M1_T: graphTitle = "Motor 1 Temp"; break;

            case GRAPH_M2_VDC: graphTitle = "Motor 2 Voltage"; break;
            case GRAPH_M2_IDC: graphTitle = "Motor 2 Current"; break;
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
            case GRAPH_M1_VDC: max_val = 200; history = d->motor1_dc_voltage_history; break;
            case GRAPH_M1_IDC: max_val = 200; history = d->motor1_dc_current_history; break;
            case GRAPH_M1_IAC: max_val = 200; history = d->motor1_ac_current_history; break;
            case GRAPH_M1_T: max_val = 80; history = d->motor1_temp_history; break;

            case GRAPH_M2_VDC: max_val = 200; history = d->motor2_dc_voltage_history; break;
            case GRAPH_M2_IDC: max_val = 200; history = d->motor2_dc_current_history; break;
            case GRAPH_M2_IAC: max_val = 200; history = d->motor2_ac_current_history; break;
            case GRAPH_M2_T: max_val = 80; history = d->motor2_temp_history; break;

            case GRAPH_TPS: max_val = 100; history = d->tps_history; break;
            case GRAPH_STEER: max_val = 180; history = d->steering_angle_history; break;
            case GRAPH_FRONT_BRK: max_val = 100; history = d->brake_front_history; break;
            case GRAPH_REAR_BRK: max_val = 100; history = d->brake_rear_history; break;

            case GRAPH_FL_SPD: max_val = 300; history = d->wheel_speed_fl_history; break;
            case GRAPH_FR_SPD: max_val = 300; history = d->wheel_speed_fr_history; break;
            case GRAPH_RL_SPD: max_val = 300; history = d->wheel_speed_rl_history; break;
            case GRAPH_RR_SPD: max_val = 300; history = d->wheel_speed_rr_history; break;
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
            case GRAPH_M1_VDC: current_val = d->motor1_dc_voltage; break;
            case GRAPH_M1_IDC: current_val = d->motor1_dc_current; break;
            case GRAPH_M1_IAC: current_val = d->motor1_ac_current; break;
            case GRAPH_M1_T: current_val = d->motor1_temp; break;

            case GRAPH_M2_VDC: current_val = d->motor2_dc_voltage; break;
            case GRAPH_M2_IDC: current_val = d->motor2_dc_current; break;
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


        static int counter = 0;
        

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

        // eve.TAG(50);
        // eve.CMD_BUTTON(170, 220, 120, 40, 26, 0, "");
        // eve.TAG(51);
        // eve.CMD_BUTTON(320, 220, 120, 40, 26, 0, "");

        eve.TAG(255);
    }

    eve.DISPLAY();

    eve.CMD_SWAP();

    eve.LIB_EndCoProList();
}
