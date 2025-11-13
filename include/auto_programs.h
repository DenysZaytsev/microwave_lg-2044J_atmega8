#ifndef AUTO_PROGRAMS_H_
#define AUTO_PROGRAMS_H_

#include "microwave_firmware.h"

// ============================================================================
// --- 🥩 ЗОВНІШНІ ОГОЛОШЕННЯ ТАБЛИЦЬ PROGMEM ---
// ============================================================================
// (Визначені в auto_programs.c, використовуються в microwave_firmware.c)

extern const FlipSchedule def_meat_flips[] PROGMEM;
extern const FlipSchedule def_poultry_flips[] PROGMEM;
extern const FlipSchedule def_fish_flips[] PROGMEM;
extern const AutoProgramEntry ac1_potato[] PROGMEM;
extern const AutoProgramEntry ac2_fresh_veg[] PROGMEM;
extern const AutoProgramEntry ac3_frozen_veg[] PROGMEM;
extern const AutoProgramEntry def1_meat[] PROGMEM;
extern const AutoProgramEntry def2_poultry[] PROGMEM;
extern const AutoProgramEntry def3_fish[] PROGMEM;
extern const AutoProgramEntry def4_bread[] PROGMEM;


// ============================================================================
// --- 🟨 ПРОТОТИПИ ФУНКЦІЙ ---
// ============================================================================
// (Решта файлу без змін)
// ... (Прототипи calculate_flip_schedule, get_program_settings, і т.д.) ...
void calculate_flip_schedule(uint8_t program_num, uint16_t weight);
void initiate_flip_pause(void);
void resume_after_flip(void);
void check_flip_required(void);
void get_program_settings(const AutoProgramEntry* table, uint8_t len, uint16_t weight);


#endif // AUTO_PROGRAMS_H_