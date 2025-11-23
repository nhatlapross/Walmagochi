// Screen5 - Step Counter Screen
// Created for ESP32 Sui Wallet

#ifndef UI_SCREEN5_H
#define UI_SCREEN5_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_Screen5
extern void ui_Screen5_screen_init(void);
extern void ui_Screen5_screen_destroy(void);
extern lv_obj_t * ui_Screen5;
extern lv_obj_t * ui_LabelStepsTitle;
extern lv_obj_t * ui_LabelStepCount;
extern lv_obj_t * ui_LabelStepsUnit;
extern lv_obj_t * ui_ButtonResetSteps;
extern lv_obj_t * ui_LabelReset;
extern lv_obj_t * ui_Arc_Steps;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
