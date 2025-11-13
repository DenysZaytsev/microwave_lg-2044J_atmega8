#include "cooking_logic.h"
#include "auto_programs.h" // Потрібен для check_flip_required()
#include "timers_isr.h"    // Потрібен для do_long_beep()
#include "display_driver.h" // Потрібен для set_colon_mode()

// ============================================================================
// --- 🟨 РЕАЛІЗАЦІЯ ФУНКЦІЙ ---
// ============================================================================

void set_magnetron(bool on) {
    if (on) {
        uint32_t elapsed = g_millis_counter - g_magnetron_last_off_timestamp_ms;
        if (g_magnetron_last_off_timestamp_ms != 0 && elapsed < ((uint32_t)MIN_SAFE_OFF_TIME_SEC * 1000UL)) {
            return; 
        }
        g_magnetron_is_on = true;
        #if (ZVS_MODE == 0)
            MAGNETRON_PORT |= MAGNETRON_BIT;
        #else
            g_magnetron_request = true;
        #endif
    } else {
        if (g_magnetron_is_on) {
            g_magnetron_last_off_timestamp_ms = g_millis_counter;
        }
        g_magnetron_is_on = false;
        #if (ZVS_MODE == 0)
            MAGNETRON_PORT &= ~MAGNETRON_BIT;
        #else
            g_magnetron_request = false; 
            MAGNETRON_PORT &= ~MAGNETRON_BIT;
        #endif
    }
}

void set_fan(bool on) { if (on) FAN_PORT |= FAN_BIT; else FAN_PORT &= ~FAN_BIT; }

void setup_hardware() {
    DDRD &= ~ZVS_BIT; 
    CDD_DDR &= ~CDD_BIT; 
    CDD_PORT |= CDD_BIT; 
    MAGNETRON_DDR |= MAGNETRON_BIT; MAGNETRON_PORT &= ~MAGNETRON_BIT;
    FAN_DDR |= FAN_BIT; FAN_PORT &= ~FAN_BIT;
    BEEPER_DDR |= BEEPER_BIT; BEEPER_PORT &= ~BEEPER_BIT;
}
#if (ZVS_MODE == 2)
void enter_sleep_mode() { 
    // Потрібно викликати reset_to_idle() з основного модуля
    // Ця реалізація спрощена
    g_state = STATE_SLEEPING; 
    set_colon_mode(COLON_OFF); 
    set_sleep_mode(SLEEP_MODE_IDLE); 
    sleep_enable();
}
void wake_up_from_sleep() { 
    sleep_disable(); 
    g_state = STATE_IDLE; 
    set_colon_mode(COLON_BLINK_SLOW);
}
#endif

void calculate_pwm_on_time() {
    if (g_cook_power_level == 0) { 
        g_pwm_on_time_seconds = g_pwm_cycle_duration; // Max power (700W)
        return; 
    }

    uint16_t watts = power_levels_watt[g_cook_power_level];
    uint16_t on_time = (uint16_t)(((uint32_t)watts * g_pwm_cycle_duration) / 700);

    uint8_t min_on = MIN_SAFE_ON_TIME_SEC;
    uint8_t max_on = g_pwm_cycle_duration;
    
    if (g_pwm_cycle_duration > MIN_SAFE_OFF_TIME_SEC) {
         max_on = g_pwm_cycle_duration - MIN_SAFE_OFF_TIME_SEC;
    } else {
         max_on = 0; 
    }

    if (min_on > max_on) {
        on_time = max_on; 
    } else {
        if (on_time < min_on) on_time = min_on;
        if (on_time > max_on) on_time = max_on;
    }

    g_pwm_on_time_seconds = (uint8_t)on_time;
}

void recalculate_adaptive_pwm() {
    if (g_cook_original_total_time < MAGNETRON_COAST_TIME_SEC && g_cook_power_level != 0) { 
        g_pwm_cycle_duration = g_cook_original_total_time; 
    } 
    else if (g_cook_original_total_time < ADAPTIVE_PWM_THRESHOLD_SEC) { g_pwm_cycle_duration = g_cook_original_total_time; } 
    else { g_pwm_cycle_duration = 30; }
    
    calculate_pwm_on_time();
    
    g_pwm_cycle_counter_seconds = 0;
}

bool start_cooking_cycle() {
    if (CDD_PIN & CDD_BIT) {
        do_short_beep();
        // reset_to_idle(); // Небезпечно викликати звідси,
        // Головний цикл має перевірити 'false' і викликати reset_to_idle()
        return false;
    }

    set_fan(true); 
    g_cook_original_total_time = g_cook_time_total_sec; 
    recalculate_adaptive_pwm(); 
    
    set_magnetron(true);
    
    g_state = STATE_COOKING; 
    set_colon_mode(COLON_ON); 
    return true;
}

void resume_cooking() {
    if (!(CDD_PIN & CDD_BIT)) { 
        g_state=STATE_COOKING; 
        set_colon_mode(COLON_ON); 
        set_fan(true); 
        
        bool should_be_on = (g_pwm_cycle_counter_seconds < g_pwm_on_time_seconds);
        if (should_be_on && 
            (g_cook_original_total_time >= ADAPTIVE_PWM_THRESHOLD_SEC) && 
            (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) && 
            (g_cook_power_level != 0))
        {
            set_magnetron(false); // Coasting
        } 
        else if (should_be_on) 
        {
            set_magnetron(true);
        } 
        else 
        {
            set_magnetron(false);
        }
        
        g_timer_ms = 0; // (v2.7.0) Скидаємо таймер
    }
}


void update_cook_timer() {
    if (g_state == STATE_COOKING && g_cook_time_total_sec > 0) {
        if (g_cook_time_total_sec <= 3 && g_stage2_time_sec == 0) do_long_beep();
        
        g_cook_time_total_sec--; 
        check_flip_required();
        
        bool should_be_on = (g_pwm_cycle_counter_seconds < g_pwm_on_time_seconds);
        if (should_be_on && 
            (g_cook_original_total_time >= ADAPTIVE_PWM_THRESHOLD_SEC) && 
            (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) && 
            (g_cook_power_level != 0))
        {
            set_magnetron(false);
        } 
        else if (should_be_on) 
        {
            set_magnetron(true);
        } 
        else 
        {
            set_magnetron(false);
        }
        
        g_pwm_cycle_counter_seconds++; 
        if (g_pwm_cycle_counter_seconds >= g_pwm_cycle_duration) g_pwm_cycle_counter_seconds = 0;
        
        if (g_cook_time_total_sec == 0) {
            if (g_stage2_time_sec > 0) { 
                g_state = STATE_STAGE2_TRANSITION;
                set_colon_mode(COLON_OFF); 
                set_magnetron(false); 
                set_fan(false);
            }
            else { 
                g_state=STATE_FINISHED; 
                set_colon_mode(COLON_OFF); 
                g_post_cook_timer_ms = 0; 
                g_post_cook_sec_counter = 0; 
                set_magnetron(false); 
                set_fan(false); 
            }
        }
    }
}