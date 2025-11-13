#include "auto_programs.h"
#include "cooking_logic.h" // Потрібен для set_magnetron/set_fan
#include "timers_isr.h"    // Потрібен для do_flip_beep

// ============================================================================
// --- 🥩 ТАБЛИЦІ (PROGMEM) ---
// ============================================================================
const FlipSchedule def_meat_flips[] PROGMEM = { {300, 1, {50,0,0,0,0}}, {500, 2, {33,67,0,0,0}}, {750, 2, {33,67,0,0,0}}, {1000, 3, {25,50,75,0,0}}, {1500, 3, {25,50,75,0,0}}, {2000, 4, {20,40,60,80,0}}, {4000, 5, {20,40,60,80,99}} };
const FlipSchedule def_poultry_flips[] PROGMEM = { {300, 1, {50,0,0,0,0}}, {500, 2, {33,67,0,0,0}}, {750, 2, {33,67,0,0,0}}, {1000, 3, {25,50,75,0,0}}, {1500, 3, {25,50,75,0,0}}, {2000, 4, {20,40,60,80,0}}, {4000, 5, {20,40,60,80,99}} };
const FlipSchedule def_fish_flips[] PROGMEM = { {300, 1, {50,0,0,0,0}}, {400, 1, {50,0,0,0,0}}, {600, 2, {40,80,0,0,0}}, {750, 2, {40,80,0,0,0}}, {1000, 2, {33,67,0,0,0}} };
const AutoProgramEntry PROGMEM ac1_potato[] = { {100,120,0,false}, {200,210,0,false}, {400,360,0,false}, {600,510,0,false}, {800,660,0,false}, {1000,780,0,false} };
const AutoProgramEntry PROGMEM ac2_fresh_veg[] = { {100,90,0,false}, {200,180,0,false}, {400,300,0,false}, {600,420,0,false}, {800,540,0,false} };
const AutoProgramEntry PROGMEM ac3_frozen_veg[] = { {100,120,0,false}, {200,240,0,false}, {400,420,0,false}, {600,570,0,false}, {800,720,0,false} };
const AutoProgramEntry PROGMEM def1_meat[] = { {100,120,3,true}, {500,600,3,true}, {1000,1260,3,true}, {2000,2760,3,true}, {3000,4500,3,true}, {4000,5999,3,true} };
const AutoProgramEntry PROGMEM def2_poultry[] = { {100,120,3,true}, {500,570,3,true}, {1000,1200,3,true}, {2000,2640,3,true}, {3000,4320,3,true}, {4000,5999,3,true} };
const AutoProgramEntry PROGMEM def3_fish[] = { {100,90,3,true}, {500,420,3,true}, {1000,900,3,true}, {2000,1920,3,true}, {3000,3000,3,true}, {4000,4200,3,true} };
const AutoProgramEntry PROGMEM def4_bread[] = { {100,40,4,false}, {200,70,4,false}, {300,100,4,false}, {400,130,4,false}, {500,150,4,false} };


// ============================================================================
// --- 🟨 РЕАЛІЗАЦІЯ ФУНКЦІЙ ---
// ============================================================================

void calculate_flip_schedule(uint8_t program_num, uint16_t weight) {
    memset((void*)&g_defrost_flip_info, 0, sizeof(g_defrost_flip_info));
    const FlipSchedule* flip_table = NULL; 
    uint8_t table_len = 0;
    
    if (program_num == 1) { 
        flip_table = def_meat_flips; 
        table_len = sizeof(def_meat_flips) / sizeof(FlipSchedule); 
    } 
    else if (program_num == 2) { 
        flip_table = def_poultry_flips; 
        table_len = sizeof(def_poultry_flips) / sizeof(FlipSchedule); 
    } 
    else if (program_num == 3) { 
        flip_table = def_fish_flips; 
        table_len = sizeof(def_fish_flips) / sizeof(FlipSchedule); 
    } 
    else return;
    
    FlipSchedule sched;
    for (uint8_t i = 0; i < table_len; i++) { 
        memcpy_P(&sched, &flip_table[i], sizeof(FlipSchedule)); 
        if (weight <= sched.weight_g) break; 
    }
    
    g_defrost_flip_info.num_flips_total = sched.num_flips;
    uint16_t total_time = g_cook_time_total_sec; 
    for (uint8_t i = 0; i < sched.num_flips; i++) { 
        if (sched.flip_percentages[i] > 0 && sched.flip_percentages[i] < 100) {
            g_defrost_flip_info.flip_times_sec[i] = total_time - (((uint32_t)total_time * sched.flip_percentages[i]) / 100); 
        }
    }
}

void initiate_flip_pause() { 
    set_magnetron(false); 
    set_fan(false); 
    g_state = STATE_FLIP_PAUSE; 
    g_flip_beep_timeout_ms = 5000; 
    do_flip_beep(); 
}

void resume_after_flip() { 
    g_defrost_flip_info.next_flip_index++; 
    g_state = STATE_COOKING; 
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

void check_flip_required() {
    if (g_active_auto_program_type != PROGRAM_DEFROST || g_defrost_flip_info.next_flip_index >= g_defrost_flip_info.num_flips_total) return;
    
    uint16_t next_flip_time = g_defrost_flip_info.flip_times_sec[g_defrost_flip_info.next_flip_index];
    if (next_flip_time > 0 && g_cook_time_total_sec == next_flip_time) 
        initiate_flip_pause();
}

void get_program_settings(const AutoProgramEntry* table, uint8_t len, uint16_t weight) {
    AutoProgramEntry entry; 
    for (uint8_t i=0; i<len; i++) { 
        memcpy_P(&entry, &table[i], sizeof(AutoProgramEntry)); 
        if (i==(len-1) || weight < pgm_read_word(&table[i+1].weight_g)) 
            break; 
    }
    g_cook_time_total_sec=entry.time_sec; 
    g_cook_power_level=entry.power_level;
}