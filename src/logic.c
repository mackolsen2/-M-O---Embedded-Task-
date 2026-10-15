// This is the only file you should edit.

#include "logic.h"

SystemOutputs evaluate_system_state(BmsData bms, BatteryTempData batt_temp,
                                     InverterData inverter, PvData pv,
                                     GridData grid, RelayState relay,
                                     DoorSwitchData door) {
    SystemOutputs out = {0};
    bool fault = false;

// Get the current system time to check whether sensor data is stale.
    uint32_t now_ms = read_system_time_ms();

    // 1. Critical communications must be working.
    if (!bms.comms_ok || !inverter.comms_ok) {
        fault = true;
    }

    // 2. Critical sensor/ECU readings must be recent.
    // Unsigned subtraction also handles normal uint32_t timer rollover.
    if ((uint32_t)(now_ms - bms.last_update_ms) > 1000U ||
        (uint32_t)(now_ms - inverter.last_update_ms) > 1000U) {
        fault = true;
    }

    // 3. Check every battery cell voltage.
    for (int i = 0; i < BMS_NUM_CELLS; i++) {
        if (bms.cell_voltages_mv[i] < 3000.0f ||
            bms.cell_voltages_mv[i] > 4200.0f) {
            fault = true;
        }
    }

    // 4. The independent battery temperature sensor must be valid
    // and the battery must remain below the assumed temperature limit.
    if (!batt_temp.valid || batt_temp.temp_c >= 60.0f) {
        fault = true;
    }

    // 5. A triggered door/tamper switch is treated as a safety fault.
    if (door.triggered) {
        fault = true;
    }

    // 6. Relay command and physical feedback must agree.
    if (relay.commanded_closed != relay.feedback_closed) {
        fault = true;
    }

    // 7. If the grid is present, check its voltage and frequency.
    // If the grid is absent, it is acceptable only when the relay is open.
    if (grid.present) {
        if (grid.voltage_v < 207.0f || grid.voltage_v > 253.0f ||
            grid.frequency_hz < 49.0f || grid.frequency_hz > 51.0f) {
            fault = true;
        }
    } else if (relay.commanded_closed || relay.feedback_closed) {
        fault = true;
    }

    // 8. Decide the system state and signal any fault.
    if (fault) {
        out.state = SYSTEM_OFF;
        out.buzzer_on = true;
        out.notification_flag = true;
    } else {
        out.state = SYSTEM_ON;
        out.buzzer_on = false;
        out.notification_flag = false;
    }

    return out;
}
