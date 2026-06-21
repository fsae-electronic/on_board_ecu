# TMS570 On-Board ECU

Firmware para la ECU on-board basada en TI TMS570LS1224. El proyecto recibe y transmite CAN, actualiza un dashboard en la Valuation Board, lee botones GIO y usa UART por XDS110 para trazas.

El punto de entrada activo es [source/sys_main.cpp](source/sys_main.cpp).

## CAN

Todos los frames usan 8 bytes. El orden de bytes en la tabla es el orden del payload en el bus: `b0` es el primer byte y `b7` el ultimo.

### Message boxes

| Box | Tipo | CAN ID | Estructura |
| --- | --- | ---: | --- |
| 1 | RX | `0x501` | `tps_data_t` |
| 2 | RX | `0x502` | `front_data_t` |
| 3 | RX | `0x503` | `current_data_t` |
| 4 | RX | `0x504` | `rear_data_t` |
| 5 | RX | `0x181` | `driver_status_t` |
| 6 | RX | `0x182` | `driver_status_t` |
| 7 | RX | `0x281` | `motor_data_t` |
| 8 | RX | `0x282` | `motor_data_t` |
| 9 | RX | `0x381` | `driver_data_t` |
| 10 | RX | `0x382` | `driver_data_t` |
| 11 | RX | `0x401` | `main_ecu_data_t` |
| 12 | RX | `0x700` | `canopen_heartbeat_t` |
| 17 | TX | `0x402` | `buttons_data_t` |

### Frames recibidos

| CAN ID | Estructura | Bytes |
| --- | --- | --- |
| `0x501` | `tps_data_t` | `b0-b1 = tps_1`, `b2-b3 = tps_2`, `b4-b7 = libre` |
| `0x502` | `front_data_t` | `b0-b1 = front_left_speed`, `b2-b3 = front_right_speed`, `b4-b5 = direction`, `b6-b7 = front_brake_pressure` |
| `0x503` | `current_data_t` | `b0-b1 = ac_current_1`, `b2-b3 = ac_current_2`, `b4-b5 = dc_current_1`, `b6-b7 = dc_current_2` |
| `0x504` | `rear_data_t` | `b0-b1 = rear_brake_pressure`, `b2-b7 = libre` |
| `0x181` | `driver_status_t` | `b0-b1 = status_word`, `b2-b3 = warning`, `b4-b5 = error`, `b6-b7 = libre` |
| `0x182` | `driver_status_t` | igual que `0x181` |
| `0x281` | `motor_data_t` | `b0-b1 = dc_voltage`, `b2-b3 = rated_current`, `b4 = temp`, `b5-b7 = libre` |
| `0x282` | `motor_data_t` | igual que `0x281` |
| `0x381` | `driver_data_t` | `b0 = driver_temp`, `b1 = padding`, `b2-b3 = driver_voltage`, `b4-b7 = libre` |
| `0x382` | `driver_data_t` | igual que `0x381` |
| `0x700` | `canopen_heartbeat_t` | `b0 = canopen_state`, `b1-b7 = libre` |
| `0x401` | `main_ecu_data_t` | `b0 = tps`, `b1 = mode`, `b2 = error_code`, `b3-b7 = libre` |


### Frame transmitido

| CAN ID | Estructura | Bytes |
| --- | --- | --- |
| `0x402` | `buttons_data_t` | `b0 = drive_enabled`, `b1 = traction_on`, `b2 = mode`, `b3 = telemetry_enabled`, `b4-b7 = libre` |

### CANopen heartbeat (ID `0x700`)

| Byte 0 | Estado |
| ---: | --- |
| `0x00` | Bootup |
| `0x05` | Operational |
| `0x7F` | Pre-operational |
| `0x04` | Stopped |

## Resumen corto

- `tps_data_t`: TPS doble de pedal.
- `front_data_t` y `rear_data_t`: velocidades/direccion y presiones de freno.
- `current_data_t`: corrientes AC/DC de ambos motores.
- `driver_status_t`: estado, warning y error del driver.
- `motor_data_t`: tension, corriente y temperatura del motor.
- `driver_data_t`: temperatura y tension del driver.
- `main_ecu_data_t` y `buttons_data_t`: estado interno del sistema y botones que publica el on-board ECU.

## Warnings y errores

Los valores simbolicos estan en [source/dashboard.h](source/dashboard.h). En el dashboard se copian tal como llegan en `driver_status_t` y `main_ecu_data_t`.

### Driver status word

| Nombre | Valor |
| --- | ---: |
| `READY_TO_SWITCH_ON` | `0` |
| `SWITCHED_ON` | `1` |
| `OPERATION_ENABLED` | `2` |
| `FAULT` | `4` |
| `VOLTAGE_ENABLED` | `8` |
| `QUICK_STOP` | `16` |
| `SWITCH_ON_DISABLED` | `32` |
| `WARNING` | `64` |
| `MFR_SPECIFIC` | `128` |
| `REMOTE` | `256` |
| `TARGET_REACHED` | `512` |
| `INTERNAL_LIMIT_ACTIVE` | `1024` |
| `OTHER_STATUS` | `2048` |

### Driver warnings

| Nombre | Valor |
| --- | ---: |
| `NO_WARNING` | `1` |
| `CONTROLLER_TEMPERATURE_EXCEEDED` | `2` |
| `MOTOR_TEMPERATURE_EXCEEDED` | `4` |
| `DC_LINK_UNDERVOLTAGE` | `8` |
| `DC_LINK_OVERVOLTAGE` | `16` |
| `STALL_PROTECTION` | `32` |
| `MAX_VELOCITY_EXCEEDED` | `64` |
| `BMS_PROPOSED_POWER` | `128` |
| `CAPACITOR_TEMPERATURE_EXCEEDED` | `256` |
| `I2T_PROTECTION` | `512` |
| `FIELD_WEAKNESS` | `1024` |
| `OTHER_WARNING` | `2048` |

### Driver errors

| Nombre | Valor |
| --- | ---: |
| `NO_FAULT` | `0` |
| `ERROR_CURRENT_A` | `0xFF01` |
| `ERROR_CURRENT_B` | `0xFF02` |
| `ERROR_HS_FET` | `0xFF03` |
| `ERROR_LS_FET` | `0xFF04` |
| `ERROR_DRV_LS_L1` | `0xFF05` |
| `ERROR_DRV_LS_L2` | `0xFF06` |
| `ERROR_DRV_LS_L3` | `0xFF07` |
| `ERROR_DRV_HS_L1` | `0xFF08` |
| `ERROR_DRV_HS_L2` | `0xFF09` |
| `ERROR_DRV_HS_L3` | `0xFF0A` |
| `ERROR_MOTOR_FEEDBACK` | `0xFF0B` |
| `ERROR_DC_LINK_UNDERVOLTAGE` | `0xFF0C` |
| `ERROR_PULS_MODE_FINISHED` | `0xFF0D` |
| `ERROR_APP_ERROR` | `0xFF0E` |
| `ERROR_STO_ERROR` | `0xFF0F` |
| `ERROR_CONTROLLER_OVERTEMPERATURE` | `0xFF10` |
| `ERROR_DC_LINK_OVERVOLTAGE` | `0x3210` |
| `ERROR_UNKNOWN` | `0xFFFF` |

### Main ECU errors

| Nombre | Valor |
| --- | ---: |
| `MAIN_ECU_NO_ERROR` | `0` |
| `MAIN_ECU_TPS_IMPLAUSIBILITY` | `1` |
| `MAIN_ECU_BREAK_IMPLAUSIBILITY` | `2` |
| `MAIN_ECU_SAFETY_SHUTDOWN` | `3` |
| `MAIN_ECU_OTHER_ERROR` | `4` |
