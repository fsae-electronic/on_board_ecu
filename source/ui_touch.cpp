#include "ui_touch.h"
#include "pages.h"
extern "C" {
#include "ti_fee.h"
}
// EEPROM storage structure for touch calibration only
struct EEPROMData {
    union
    {
        uint8_t raw[28];
        struct
        {
            uint32_t magic;
            uint32_t touch_a;
            uint32_t touch_b;
            uint32_t touch_c;
            uint32_t touch_d;
            uint32_t touch_e;
            uint32_t touch_f;
        } values;
    };
};

static const uint32_t EEPROM_MAGIC = 0xA5A5A5A5;

bool loadCalibration(Bridgetek_EVE2 &eve) 
{
    EEPROMData e;

    while(TI_Fee_GetStatus(0) != IDLE)
    {
        TI_Fee_MainFunction();
    }
    TI_Fee_ReadSync(1, 0, e.raw, sizeof(EEPROMData));

    if (e.values.magic != EEPROM_MAGIC) 
    {
        return false;
    }
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_A, e.values.touch_a);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_B, e.values.touch_b);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_C, e.values.touch_c);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_D, e.values.touch_d);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_E, e.values.touch_e);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_F, e.values.touch_f);

    return true;
}

void saveCalibration(Bridgetek_EVE2 &eve) 
{
    while(TI_Fee_GetStatus(0) != IDLE)
    {
        TI_Fee_MainFunction();
    }
        
    EEPROMData e;
    e.values.magic = EEPROM_MAGIC;
    e.values.touch_a = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_A);
    e.values.touch_b = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_B);
    e.values.touch_c = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_C);
    e.values.touch_d = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_D);
    e.values.touch_e = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_E);
    e.values.touch_f = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_F);
    TI_Fee_WriteAsync(1, e.raw);
    while(TI_Fee_GetStatus(0) != IDLE)
    {
        TI_Fee_MainFunction();
    }

}




void delay (int ticks)
{
    for (volatile int i = 0; i < ticks * 1000; i++);
}


// Detecta si hay toque en la pantalla
static int eve_key_detect(Bridgetek_EVE2 &eve)
{
    int key_detect = 0;
    if (!(eve.LIB_MemRead16(eve.REG_TOUCH_SCREEN_XY) & 0x8000)) {
        key_detect = 1;
    }
    return key_detect;
}

// Lee el tag del toque
int eve_read_tag(Bridgetek_EVE2 &eve, uint8_t *key)
{
    uint8_t Read_tag;
    int key_detect = 0;

    Read_tag = eve.LIB_MemRead8(eve.REG_TOUCH_TAG);
    if (!(eve.LIB_MemRead16(eve.REG_TOUCH_SCREEN_XY) & 0x8000)) 
    {
        key_detect = 1;
        *key = Read_tag;
    }
    
    // Resetear TAG a 0 para permitir detectar el siguiente toque
    eve.LIB_MemWrite8(eve.REG_TOUCH_TAG, 0);

    return key_detect;
}

// Calibración de pantalla táctil
int eve_calibrate(Bridgetek_EVE2 &eve)
{
    // Esperar a que se suelte cualquier toque previo
    while (eve_key_detect(eve)) {
        delay(10);
    }
    delay(500);

    // Mostrar pantalla de calibración
    eve.LIB_BeginCoProList();
    eve.CMD_DLSTART();
    eve.CLEAR_COLOR_RGB(0, 0, 0);
    eve.CLEAR(1, 1, 1);
    eve.COLOR_RGB(255, 255, 255);
    eve.CMD_TEXT(
        eve.DISP_WIDTH()/2, 
        eve.DISP_HEIGHT()/2,
        28, 
        eve.OPT_CENTERX | eve.OPT_CENTERY,
        "Please tap on the dots"
    );
    eve.CMD_CALIBRATE(0);
    eve.LIB_EndCoProList();
    
    if (eve.LIB_AwaitCoProEmpty() != 0) {
        return -1;
    }

    // Guardar los parámetros de calibración
    uint32_t calib_a = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_A);
    uint32_t calib_b = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_B);
    uint32_t calib_c = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_C);
    uint32_t calib_d = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_D);
    uint32_t calib_e = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_E);
    uint32_t calib_f = eve.LIB_MemRead32(eve.REG_TOUCH_TRANSFORM_F);

    // Aplicar los parámetros de calibración
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_A, calib_a);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_B, calib_b);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_C, calib_c);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_D, calib_d);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_E, calib_e);
    eve.LIB_MemWrite32(eve.REG_TOUCH_TRANSFORM_F, calib_f);

    // Reset del controlador de toque
    eve.LIB_MemWrite8(eve.REG_CPURESET, 2);
    eve.LIB_MemWrite8(eve.REG_CPURESET, 0);

    delay(500);
    return 0;
}

// helper para leer posición de toque (devuelve true si hay toque)
bool eve_get_xy(Bridgetek_EVE2 &eve, uint16_t *x, uint16_t *y)
{
    uint32_t val = eve.LIB_MemRead32(eve.REG_TOUCH_SCREEN_XY);
    // bit31==1 indica sin toque
    if (val & 0x80000000)
        return false;
    *x = (val >> 16) & 0xFFFF;
    *y = val & 0xFFFF;
    return true;
}

void ui_handle_touch(Bridgetek_EVE2 &eve, dashboard_data_t *data)
{
    static uint8_t last_key = 0;
    static bool tracking_swipe = false;
    static uint16_t swipe_x0 = 0;
    static uint16_t swipe_y0 = 0;
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;

    uint8_t key;
    uint16_t tx, ty;

    if (eve_key_detect(eve)) {
        // hay contacto; actualizar swipe y tag
        eve_get_xy(eve, &tx, &ty);
        last_x = tx;
        last_y = ty;

        if (!tracking_swipe) {
            tracking_swipe = true;
            swipe_x0 = tx;
            swipe_y0 = ty;

            last_x = tx;
            last_y = ty;
        }

        if (eve_read_tag(eve, &key)) {
            last_key = key;
        }
    } else {
        // toque liberado

        if (last_key != 0) {
            // acción de botón al soltar
            switch (last_key) {
                case 1: data->traction_on = !data->traction_on; break;
                case 2: data->mode = !data->mode; break;
                case 3: data->drive_enabled = !data->drive_enabled; break;

                case 10: current_page = PAGE_GRAPH; current_graph = GRAPH_M1_VDC; break;
                case 11: current_page = PAGE_GRAPH; current_graph = GRAPH_M1_IDC; break;
                case 12: current_page = PAGE_GRAPH; current_graph = GRAPH_M1_IAC; break;
                case 13: current_page = PAGE_GRAPH; current_graph = GRAPH_M1_T; break;
                case 14: current_page = PAGE_GRAPH; current_graph = GRAPH_M2_VDC; break;
                case 15: current_page = PAGE_GRAPH; current_graph = GRAPH_M2_IDC; break;
                case 16: current_page = PAGE_GRAPH; current_graph = GRAPH_M2_IAC; break;
                case 17: current_page = PAGE_GRAPH; current_graph = GRAPH_M2_T; break;
                case 20: current_page = PAGE_GRAPH; current_graph = GRAPH_TPS; break;
                case 19: current_page = PAGE_GRAPH; current_graph = GRAPH_STEER; break;
                case 21: current_page = PAGE_GRAPH; current_graph = GRAPH_FRONT_BRK; break;
                case 22: current_page = PAGE_GRAPH; current_graph = GRAPH_REAR_BRK; break;
                case 23: current_page = PAGE_GRAPH; current_graph = GRAPH_FL_SPD; break;
                case 24: current_page = PAGE_GRAPH; current_graph = GRAPH_FR_SPD; break;
                case 25: current_page = PAGE_GRAPH; current_graph = GRAPH_RL_SPD; break;
                case 26: current_page = PAGE_GRAPH; current_graph = GRAPH_RR_SPD; break;

                case 30: current_page = PAGE_TELEMETRY; current_graph = GRAPH_NONE; break;

                case 40:
                    if(data->cal_tps_0 == 0)
                    {
                        data->cal_tps_0 = 1;
                        cal_tps_0_timer = 50;
                    }
                    break;

                case 41:
                    if(data->cal_tps_100 == 0)
                    {
                        data->cal_tps_100 = 1;
                        cal_tps_100_timer = 50;
                    }
                    break;

                case 42: data->traction_on = !data->traction_on; break;
                case 43: data->mode = !data->mode; break;
                case 44: data->drive_enabled = !data->drive_enabled; break;
                case 45: data->telemetry_enabled = !data->telemetry_enabled; break;

                case 46:
                    if(data->cal_left_steer == 0)
                    {
                        data->cal_left_steer = 1;
                        cal_left_steer_timer = 50;
                    }
                    break;

                case 47:
                    if(data->cal_right_steer == 0)
                    {
                        data->cal_right_steer = 1;
                        cal_right_steer_timer = 50;
                    }
                    break;

                case 48: break;

                case 49:
                    data->cal_screen = 1;
                    eve_calibrate(eve);
                    saveCalibration(eve);
                    data->cal_screen = 0;
                    break;

                case 50: break;
                case 51: break;
            }

            last_key = 0;
        }

        if (tracking_swipe) {

            int16_t dx = (int16_t)last_x - (int16_t)swipe_x0;
            int16_t dy = (int16_t)last_y - (int16_t)swipe_y0;

            const int16_t threshold = 60;     // distancia mínima swipe
            const int16_t max_y_move = 40;    // evita swipe diagonal

            // solo swipe si NO se presionó botón
            if (last_key == 0 && abs(dx) > threshold && abs(dy) < max_y_move) {

                if (dx > 0) {
                    // swipe derecha
                    int np = (int)current_page + 1;
                    if (np > PAGE_DEBUG) np = PAGE_RACE;
                    current_page = (ui_page_t)np;
                }
                else {
                    // swipe izquierda
                    int np = (int)current_page - 1;
                    if (np < PAGE_RACE) np = PAGE_DEBUG;
                    current_page = (ui_page_t)np;
                }
            }

            tracking_swipe = false;
        }
    }
}
