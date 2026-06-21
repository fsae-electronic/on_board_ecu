# TMS570 On-Board ECU

Firmware para la ECU on-board basada en **TI TMS570LS1224**. El proyecto integra:

- Recepcion y transmision CAN.
- Interfaz grafica en la Valuation Board.
- Entrada por botones y llaves GIO.
- Comunicacion UART por el XDS110.
- Lectura y escritura de EEPROM con TI FEE.

El punto de entrada activo del proyecto es [source/sys_main.cpp](source/sys_main.cpp). El archivo [source/sys_main.c](source/sys_main.c) sigue existiendo en el arbol generado por HALCoGen, pero no es el que debe usarse para el arranque del firmware.

## Vision general

El flujo principal es:

1. Inicializar reloj, E/S, SCI, CAN, RTI y FEE.
2. Configurar la pantalla y la logica de dashboard.
3. Recibir tramas CAN y volcar sus datos en estructuras tipadas.
4. Actualizar el dashboard y transmitir el estado de botones y modos.

El proyecto esta pensado para trabajar con mensajes CAN de 8 bytes. En la implementacion actual los payloads se acceden con uniones entre `raw[8]` y estructuras tipadas.

## UART / Consola serie

El TMS570 envia trazas por el **XDS110 Class Application/User UART**.

- Puerto correcto en Windows: normalmente el que aparece como `XDS110 Class Application/User UART`.
- En esta workspace, el puerto detectado fue `COM4`.
- El puerto `XDS110 Class Auxiliary Data Port` no es el que se usa para las trazas del firmware.

Parametros tipicos de la consola:

- 115200 baudios
- 8 bits de datos
- sin paridad
- 1 bit de stop
- sin control de flujo

Importante: varios mensajes se envian al inicio del `main`, asi que conviene abrir la consola antes de resetear la placa.

## Arquitectura de datos

Las estructuras CAN principales estan definidas en [source/data.h](source/data.h).

### Datos recibidos

| Estructura | CAN ID | Uso |
| --- | ---: | --- |
| `tps_data_t` | `0x501` | TPS y sensores asociados |
| `front_data_t` | `0x502` | Velocidades delanteras, angulo y freno delantero |
| `current_data_t` | `0x503` | Corrientes AC/DC de los dos motores |
| `rear_data_t` | `0x504` | Presion de freno trasero |
| `driver_status_t` | `0x181` / `0x182` | Estado del driver 1 y 2 |
| `motor_data_t` | `0x281` / `0x282` | Datos de potencia y temperatura de motor 1 y 2 |
| `driver_data_t` | `0x381` / `0x382` | Temperatura y tension de driver 1 y 2 |
| `main_ecu_data_t` | `0x401` | Mensaje asociado a la Main ECU |

### Datos transmitidos

| Estructura | CAN ID | Uso |
| --- | ---: | --- |
| `buttons_data_t` | `0x402` | Estado de botones y modos que el on-board ECU publica |

## Mapa CAN del firmware

La inicializacion de CAN en [source/can.c](source/can.c) configura mensajes de 8 bytes y filtros exactos por ID. Los IDs configurados son estos:

### Mensajes recibidos por CAN

| Message box | CAN ID | Estructura | Descripcion |
| --- | ---: | --- | --- |
| 1 | `0x501` | `tps_data_t` | TPS |
| 2 | `0x502` | `front_data_t` | Datos delanteros |
| 3 | `0x503` | `current_data_t` | Corrientes |
| 4 | `0x504` | `rear_data_t` | Freno trasero |
| 5 | `0x181` | `driver_status_t` | Driver 1 status |
| 6 | `0x182` | `driver_status_t` | Driver 2 status |
| 7 | `0x281` | `motor_data_t` | Motor 1 data |
| 8 | `0x282` | `motor_data_t` | Motor 2 data |
| 9 | `0x381` | `driver_data_t` | Driver 1 data |
| 10 | `0x382` | `driver_data_t` | Driver 2 data |
| 11 | `0x401` | `main_ecu_data_t` | Frame de la Main ECU |

### Mensajes transmitidos por CAN

| Message box | CAN ID | Estructura | Descripcion |
| --- | ---: | --- | --- |
| 12 | `0x402` | `buttons_data_t` | Estado de botones y modos del on-board ECU |

### Mensajes de test

Si se activa `test_main_ecu_periodic()` en [source/test_main_ecu.cpp](source/test_main_ecu.cpp), se transmiten frames de prueba adicionales:

| Message box | CAN ID | Buffer | Contenido |
| --- | ---: | --- | --- |
| 17 | configurado en test | `sensors_msg` | TPS, freno delantero, freno trasero, direccion |
| 18 | configurado en test | `wheel_speed_msg` | Velocidades de rueda |
| 19 | configurado en test | `motor1_msg` | Tension, corriente, temperatura motor 1 |
| 20 | configurado en test | `motor2_msg` | Tension, corriente, temperatura motor 2 |
| 21 | configurado en test | `status_msg` | Fallas motor 1, motor 2 y drive state |
| 22 | configurado en test | `dynamic_msg` | RPM, tension bateria y corriente bateria |

Nota: el modulo de test esta en el proyecto, pero en [source/sys_main.cpp](source/sys_main.cpp) su llamada periodica esta comentada.
Los frames de test se documentan por message box y payload porque en la configuracion CAN visible del proyecto no aparece su arbitraje final en [source/can.c](source/can.c).

## Significado de cada payload

### 1) `tps_data_t` - CAN ID `0x501`

Estructura interna:

- `tps_1` `uint16_t`
- `tps_2` `uint16_t`

Uso:

- `dashboard_data.tps_1` toma `tps_1`.
- `dashboard_data.tps_2` toma `tps_2`.
- `dashboard_data.tps` es el promedio de ambos sensores.

Interpretacion funcional:

- Sensor de pedal acelerador A.
- Sensor de pedal acelerador B.
- El promedio se usa como valor final de TPS en el dashboard.

### 2) `front_data_t` - CAN ID `0x502`

Estructura interna:

- `front_left_speed` `uint16_t`
- `front_right_speed` `uint16_t`
- `direction` `uint16_t`
- `front_brake_pressure` `uint16_t`

Uso:

- Velocidad rueda delantera izquierda.
- Velocidad rueda delantera derecha.
- Angulo/direccion de la direccion delantera.
- Presion de freno delantero.

En el dashboard se copia a:

- `wheel_speed_fl`
- `wheel_speed_fr`
- `steering_angle`
- `brake_front`

### 3) `current_data_t` - CAN ID `0x503`

Estructura interna:

- `ac_current_1` `uint16_t`
- `ac_current_2` `uint16_t`
- `dc_current_1` `uint16_t`
- `dc_current_2` `uint16_t`

Uso:

- Corriente AC motor 1.
- Corriente AC motor 2.
- Corriente DC motor 1.
- Corriente DC motor 2.

En el dashboard:

- `motor1_ac_current`
- `motor1_dc_current`
- `motor2_ac_current`
- `motor2_dc_current`

### 4) `rear_data_t` - CAN ID `0x504`

Estructura interna:

- `rear_brake_pressure` `uint16_t`

Uso:

- Presion de freno trasero.

En el dashboard:

- `brake_rear`

### 5) `driver_status_t` - CAN IDs `0x181` y `0x182`

Estructura interna:

- `status_word` `uint16_t`
- `warning` `uint16_t`
- `error` `uint16_t`

Interpretacion:

- `status_word` contiene el estado operativo del driver.
- `warning` contiene banderas de advertencia.
- `error` contiene el codigo de falla activo.

Mapeo de estado en `dashboard.h`:

#### Estado (`driver_status_word_t`)

- `READY_TO_SWITCH_ON = 0`
- `SWITCHED_ON = 1`
- `OPERATION_ENABLED = 2`
- `FAULT = 4`
- `VOLTAGE_ENABLED = 8`
- `QUICK_STOP = 16`
- `SWITCH_ON_DISABLED = 32`
- `WARNING = 64`
- `MFR_SPECIFIC = 128`
- `REMOTE = 256`
- `TARGET_REACHED = 512`
- `INTERNAL_LIMIT_ACTIVE = 1024`
- `OTHER_STATUS = 2048`

#### Warning (`driver_warning_code_t`)

- `NO_WARNING = 1`
- `CONTROLLER_TEMPERATURE_EXCEEDED = 2`
- `MOTOR_TEMPERATURE_EXCEEDED = 4`
- `DC_LINK_UNDERVOLTAGE = 8`
- `DC_LINK_OVERVOLTAGE = 16`
- `STALL_PROTECTION = 32`
- `MAX_VELOCITY_EXCEEDED = 64`
- `BMS_PROPOSED_POWER = 128`
- `CAPACITOR_TEMPERATURE_EXCEEDED = 256`
- `I2T_PROTECTION = 512`
- `FIELD_WEAKNESS = 1024`
- `OTHER_WARNING = 2048`

#### Error (`driver_error_code_t`)

- `NO_FAULT = 0`
- `ERROR_CURRENT_A = 0xFF01`
- `ERROR_CURRENT_B = 0xFF02`
- `ERROR_HS_FET = 0xFF03`
- `ERROR_LS_FET = 0xFF04`
- `ERROR_DRV_LS_L1 = 0xFF05`
- `ERROR_DRV_LS_L2 = 0xFF06`
- `ERROR_DRV_LS_L3 = 0xFF07`
- `ERROR_DRV_HS_L1 = 0xFF08`
- `ERROR_DRV_HS_L2 = 0xFF09`
- `ERROR_DRV_HS_L3 = 0xFF0A`
- `ERROR_MOTOR_FEEDBACK = 0xFF0B`
- `ERROR_DC_LINK_UNDERVOLTAGE = 0xFF0C`
- `ERROR_PULS_MODE_FINISHED = 0xFF0D`
- `ERROR_APP_ERROR = 0xFF0E`
- `ERROR_STO_ERROR = 0xFF0F`
- `ERROR_CONTROLLER_OVERTEMPERATURE = 0xFF10`
- `ERROR_DC_LINK_OVERVOLTAGE = 0x3210`
- `ERROR_UNKNOWN = 0xFFFF`

En el dashboard:

- `driver1_warning` y `driver2_warning`
- `driver1_error` y `driver2_error`

### 6) `motor_data_t` - CAN IDs `0x281` y `0x282`

Estructura interna:

- `dc_voltage` `uint16_t`
- `rated_current` `uint16_t`
- `temp` `uint8_t`

Uso:

- Tension DC del motor.
- Corriente nominal / asociada al motor.
- Temperatura del motor.

En el dashboard:

- `motor1_dc_voltage` / `motor2_dc_voltage`
- `motor1_temp` / `motor2_temp`

### 7) `driver_data_t` - CAN IDs `0x381` y `0x382`

Estructura interna:

- `driver_temp` `uint8_t`
- `driver_voltage` `uint16_t`

Uso:

- Temperatura del driver.
- Tension del driver.

En el dashboard:

- `driver1_temp` / `driver2_temp`
- `driver1_voltage` / `driver2_voltage`

### 8) `main_ecu_data_t` - CAN ID `0x401`

Estructura interna:

- `tps` `uint8_t`
- `mode` `uint8_t`
- `error_code` `uint8_t`

Significado funcional:

- `tps`: valor resumido del acelerador.
- `mode`: modo de conduccion.
- `error_code`: codigo interno de error de la ECU principal.

Enums relacionados en el dashboard:

- `mode_t`: `NORMAL = 0`, `RACE = 1`
- `main_ecu_error_code_t`:
  - `MAIN_ECU_NO_ERROR = 0`
  - `MAIN_ECU_TPS_IMPLAUSIBILITY = 1`
  - `MAIN_ECU_BREAK_IMPLAUSIBILITY = 2`
  - `MAIN_ECU_SAFETY_SHUTDOWN = 3`
  - `MAIN_ECU_OTHER_ERROR = 4`

### 9) `buttons_data_t` - CAN ID `0x402`

Estructura interna:

- `drive_enabled` `uint8_t`
- `traction_on` `uint8_t`
- `mode` `uint8_t`
- `telemetry_enabled` `uint8_t`

Significado:

- `drive_enabled`: habilitacion de drive.
- `traction_on`: traction control activo.
- `mode`: modo de conduccion.
- `telemetry_enabled`: telemetria habilitada.

El firmware toma estos valores desde `dashboard_data` y los transmite por `canMESSAGE_BOX12`.

## Reglas de codificacion de payload

- Todos los mensajes CAN trabajan con 8 bytes.
- Los campos se copian desde o hacia el buffer `raw[8]` de cada estructura.
- El orden de bytes sigue la implementacion del compilador y la plataforma del proyecto. Si vas a decodificar desde otra herramienta, valida el endian real con una traza en el bus.
- Para los frames de estado, warnings y errores, el valor de cada campo debe interpretarse contra los enums definidos en [source/dashboard.h](source/dashboard.h).

## Flujo de runtime

### Inicializacion

En [source/sys_main.cpp](source/sys_main.cpp):

1. Se habilitan interrupciones.
2. Se inicializa SCI y se fija 115200.
3. Se inicializa CAN.
4. Se inicializa la pantalla.
5. Se inicializa TI FEE.
6. Se conectan las entradas GIO con variables del dashboard.
7. Se inicializa RTI.
8. Se levanta el dashboard.

### Recepcion CAN

Cuando llega un frame CAN:

- `canMessageNotification()` marca `new_data = true` para la estructura correspondiente.
- `update_data()` llama a `canGetData()` sobre el message box adecuado.
- Luego copia el contenido del payload a `dashboard_data`.

### Transmision CAN

`update_data()` tambien arma el frame de botones y lo transmite por `canMESSAGE_BOX12`.

## Pines y bus

El pinmux habilita `SCITX` y `SCIRX` en [source/pinmux.c](source/pinmux.c), asi que la consola serie depende del UART expuesto por el XDS110 de la board de evaluacion.

Para CAN, la configuracion principal esta en [source/can.c](source/can.c) y usa los message boxes enumerados arriba.

## Compilacion

El proyecto esta preparado para Code Composer Studio / HALCoGen con toolchain TI ARM C/C++.

Construccion tipica:

1. Abrir el proyecto en CCS.
2. Seleccionar la configuracion `Debug`.
3. Compilar.
4. Flashear en la TMS570LS1224.

## Notas de mantenimiento

- El archivo [halcogen_on_board_ecu.hcg](halcogen_on_board_ecu.hcg) sigue siendo la referencia para regenerar configuracion HALCoGen.
- Si regeneras el proyecto, revisa que `sys_main.cpp` siga siendo el entry point activo.
- Si cambias estructuras CAN, actualiza este README junto con [source/data.h](source/data.h) y [source/can.c](source/can.c).
