# TMS570 On-Board ECU

Firmware para la ECU on-board basada en TI TMS570LS1224. El proyecto recibe y transmite CAN, actualiza un dashboard en la Valuation Board, lee botones GIO y usa UART por XDS110 para trazas.

El punto de entrada activo es [source/sys_main.cpp](source/sys_main.cpp).

## UI y diagnostico implementado

Resumen de funcionalidades agregadas en el dashboard on-board para visualizacion, diagnostico y pruebas.

### Navegacion de paginas

- Navegacion por flechas izquierda/derecha (sin swipe).
- Orden actual de paginas: `RACE` -> `NORMAL` -> `TELEMETRY` -> `DEBUG` -> `FAULTS/LOG` -> `RACE`.
- La pagina `GRAPH` se abre desde `TELEMETRY` y vuelve a `TELEMETRY` con `BACK`.

Referencias: [source/pages.h](source/pages.h), [source/ui_touch.cpp](source/ui_touch.cpp), [source/dashboard.cpp](source/dashboard.cpp).

### Pagina RACE (estilo F1)

- Vista minimal con variables principales grandes:
	- `rpm`
	- `battery_voltage`
	- `battery_current`
	- `tps`
	- `brake_front`
- Se mantiene la barra de RPM superior.
- Indicador rojo de diferencia de velocidad entre ejes cuando la diferencia relativa front/rear es `>= 20%`.

Referencia: [source/dashboard.cpp](source/dashboard.cpp).

### Overlays criticos en RACE

- `WARNING`: franja amarilla con codigo del warning activo y driver.
- `FAULT`: pantalla roja completa con codigo de error activo y driver.
- Cierre manual: toque en pantalla para ocultar overlay.
- Limpieza automatica: si el estado vuelve a `NO_WARNING` o `NO_FAULT`, el overlay se limpia automaticamente.
- Prioridad visual: `FAULT` tiene prioridad sobre `WARNING`.

Referencia: [source/dashboard.cpp](source/dashboard.cpp), [source/ui_touch.cpp](source/ui_touch.cpp).

### Pagina NORMAL (layout clasico)

- Se conserva la vista clasica anterior con bloques de motores, centro y estado.
- Misma navegacion por flechas que el resto de paginas.
- Barras de presion de freno (front/rear) con escala `0-300`.

Referencia: [source/dashboard.cpp](source/dashboard.cpp).

### Pagina TELEMETRY y graficos

- Grilla 4x4 con variables en tiempo real.
- Cada celda tiene tag tactil para abrir su grafico historico (`GRAPH`).
- Variables con acceso a grafico:
	- Driver 1: VDC, IDC, AC, Temp
	- Driver 2: VDC, IDC, AC, Temp
	- Controles: TPS, Steering, Front Brake, Rear Brake
	- Ruedas: FL, FR, RL, RR
- En `GRAPH` se muestra curva historica, valor actual y escala.

Referencia: [source/dashboard.cpp](source/dashboard.cpp), [source/ui_touch.cpp](source/ui_touch.cpp).

### Pagina FAULTS/LOG

- Log circular en RAM con timestamp relativo por tick de dashboard.
- Colores por severidad:
	- `INFO`: gris
	- `WARNING`: amarillo
	- `FAULT`: rojo
- Scroll arriba/abajo y boton `CLR` para limpiar.
- Se registran transiciones de:
	- CANopen state
	- warnings y faults de ambos drivers
	- acciones de botones de `DEBUG`

Referencia: [source/dashboard.cpp](source/dashboard.cpp), [source/ui_touch.cpp](source/ui_touch.cpp).

### Mirror de logs por UART

- Cada evento log tambien se envia por SCI/UART con formato textual:
	- `[mm:ss.mmm] DRVx LEVEL mensaje`

Referencia: [source/dashboard.cpp](source/dashboard.cpp).

### Modo TEST DATA (simulacion)

- Toggle `TEST DATA ON/OFF` desde `DEBUG`.
- Genera datos sinteticos para validar UI, graficos, warnings y faults.
- Mientras `TEST DATA` esta activo, se inhibe TX CAN de comandos desde `update_data()` para no interferir con el sistema real.
- Al desactivar `TEST DATA`, se reinicia el dashboard con `init_dashboard(...)`.

Referencia: [source/test_data.cpp](source/test_data.cpp), [source/data.cpp](source/data.cpp), [source/ui_touch.cpp](source/ui_touch.cpp), [source/dashboard.cpp](source/dashboard.cpp).

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
| 11 | RX | `0x401` | `implausibility_data_t` |
| 12 | RX | `0x700` | `canopen_heartbeat_t` |
| 16 | RX | `0x403` | `main_ecu_errors_t` |
| 17 | TX | `0x402` | `buttons_data_t` |
| 18 | TX | `0x600` | `calibration_cmd_t` |

### Frames recibidos

| CAN ID | Estructura | Bytes |
| --- | --- | --- |
| `0x501` | `tps_data_t` | `b0-b1 = tps_1`, `b2-b3 = tps_2`, `b4-b7 = libre` |
| `0x502` | `front_data_t` | `b0-b1 = front_left_speed`, `b2-b3 = front_right_speed`, `b4-b5 = direction`, `b6-b7 = front_brake_pressure` |
| `0x503` | `current_data_t` | `b0-b1 = ac_current_1`, `b2-b3 = ac_current_2`, `b4-b5 = dc_current_1`, `b6-b7 = dc_current_2` |
| `0x504` | `rear_data_t` | `b0-b1 = rear_brake_pressure`, `b2-b7 = libre` |
| `0x181` | `driver_status_t` | `b0-b1 = status_word`, `b2-b3 = warning`, `b4-b5 = error`, `b6-b7 = libre` |
| `0x182` | `driver_status_t` | igual que `0x181` |
| `0x281` | `motor_data_t` | `b0-b3 = motor_velocity` (0x606C), `b4-b5 = driver_dc_voltage` (0x6079), `b6-b7 = libre` |
| `0x282` | `motor_data_t` | igual que `0x281` |
| `0x381` | `driver_data_t` | `b0-b3 = motor_rated_current` (0x6075), `b4 = driver_temp` (0x2026:1), `b5 = motor_temp` (0x2025), `b6-b7 = libre` |
| `0x382` | `driver_data_t` | igual que `0x381` |
| `0x700` | `canopen_heartbeat_t` | `b0 = canopen_state`, `b1-b7 = libre` |
| `0x401` | `implausibility_data_t` | `b0 = tps_implausibility`, `b1 = brake_implausibility`, `b2-b7 = 0x00` |
| `0x403` | `main_ecu_errors_t` | `b0 = apps_brake_plausibility`, `b1 = sensor_can_timeout` (bitmask), `b2-b7 = 0x00` (reservados) |


### Frame transmitido

| CAN ID | Estructura | Bytes |
| --- | --- | --- |
| `0x402` | `buttons_data_t` | `b0 = drive_enabled`, `b1 = traction_on`, `b2 = mode`, `b3 = telemetry_enabled`, `b4-b7 = libre` |
| `0x600` | `calibration_cmd_t` | `b0 = cmd_id`, `b1 = value`, `b2-b7 = libre` |

### Frame de calibracion (ID `0x600`)

| `cmd_id` | Accion | `value` |
| ---: | --- | --- |
| `1` | `cal_tps_0` | - |
| `2` | `cal_tps_100` | - |
| `3` | `cal_left_steer` | - |
| `4` | `cal_center_steer` | - |
| `5` | `cal_right_steer` | - |
| `6` | `cal_current_sensors` | - |
| `7` | `drive_enable` on | - |
| `8` | `drive_enable` off | - |
| `9` | `traction` on | - |
| `0xA` | `traction` off | - |
| `0xB` | `mode` normal | - |
| `0xC` | `mode` race | - |
| `0xD` | `telemetry` enable | - |
| `0xE` | `telemetry` disable | - |

### Implausibilidad (ID `0x401`)

Lo mandan `adc2can_front` y `adc2can_rear` hacia la main ECU, una sola vez por falla (no es periodico).

| Byte | Significado |
| --- | --- |
| `b0` | `1` = TPS implausibility, `0` = no |
| `b1` | `1` = brake implausibility, `0` = no |
| `b2-b7` | `0x00` |

La main ECU fuerza drive off (igual que `cmd_id = 8` en `0x600`) y queda frenada hasta recibir un `cmd_id = 7`. El on-board ECU lo escucha en el box 11: si `b0` o `b1` != 0 pone `drive_enabled = 0` **sin** mandar `0x600` ni `0x402` (asi el proximo toque del boton manda directamente `cmd_id = 7`) y muestra el aviso en pantalla hasta que se toque la pantalla o el piloto vuelva a habilitar drive (cerrar el aviso no habilita drive).

### Errores de la main ECU (ID `0x403`)

| Byte | Significado |
| --- | --- |
| `b0` | `1` = APPS/brake plausibility activa (motores detenidos), `0` = no |
| `b1` | Timeouts de CAN de sensores (bitmask): bit 0 = no llega `0x501` (TPS, `adc2can_front`), bit 1 = no llega `0x502` (freno delantero, `adc2can_front`), bit 2 = no llega `0x504` (freno trasero, `adc2can_rear`) |
| `b2-b7` | `0x00` (reservados para futuros errores de la main ECU) |

Se manda en cada cambio y cada 100 ms mientras `b0` o `b1` != 0. Sin error no se manda periodicamente. El on-board ECU lo escucha en el box 16; como respaldo, si hay un error activo y no llega ningun `0x403` por ~300 ms, limpia `b0` y `b1`.

**`b0` - APPS / Brake Pedal Plausibility (EV.4.7).** Si el freno esta apretado (presion delantera o trasera > 50 PSI) y cualquiera de los dos TPS marca mas de 25 %, la main ECU detiene los motores hasta que el acelerador baja de 5 %, con o sin freno. Despues retoma **sola**: no hace falta tocar el boton de drive ni mandar ningun `0x600` (a diferencia de `0x401`). El on-board ECU actualiza `dashboard_data.apps_brake_plausibility` y mientras esta activo muestra en cualquier pagina el aviso "APPS / BRAKE PLAUSIBILITY - SOLTAR ACELERADOR" (no se cierra con un toque). Lo registra en el log (`LOG_FAULT` al activarse, `LOG_INFO` al liberarse). No modifica `drive_enabled` ni manda `0x402`/`0x600`.

**`b1` - Sensor CAN timeout.** Un mensaje esta en timeout si no llego en mas de 100 ms (o nunca llego). Mientras `b1` != 0 la main ECU fuerza drive off y no lo acepta; cuando vuelve a 0 hay que volver a habilitar drive (`cmd_id = 7`). El on-board ECU actualiza `dashboard_data.sensor_can_timeout` y mientras es != 0:
- pone `drive_enabled = 0` en cada ciclo **sin** mandar `0x600` ni `0x402`, asi el boton queda coherente con la main ECU y se ignoran los pedidos de habilitar drive (boton o touch);
- muestra en cualquier pagina el aviso "SENSOR CAN TIMEOUT" con los sensores afectados (`TPS / FRENO DEL / FRENO TRAS`), que no se cierra con un toque y desaparece cuando `b1` vuelve a 0;
- registra en el log un `LOG_FAULT` por cada sensor que entra en timeout y un `LOG_INFO` cuando se liberan todos.

Al liberarse, drive queda en OFF: el proximo toque del boton manda directamente `cmd_id = 7`.

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
- `motor_data_t`: velocidad de motor, corriente nominal y temperatura.
- `driver_data_t`: temperatura y tension del driver.
- `implausibility_data_t`: implausibilidad de TPS/freno avisada por `adc2can_front`/`adc2can_rear` (`0x401`).
- `main_ecu_errors_t`: errores de la main ECU (`0x403`: APPS/brake plausibility EV.4.7 y timeouts de CAN de sensores).
- `buttons_data_t`: botones que publica el on-board ECU en `0x402`.
- `calibration_cmd_t`: comando de calibracion o cambio de estado de boton/modo por `cmd_id` + `value` (TX en `0x600`).

## Warnings y errores

Los valores simbolicos estan en [source/dashboard.h](source/dashboard.h). En el dashboard se copian tal como llegan en `driver_status_t`.

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

