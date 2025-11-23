// Screen5 - Step Counter Screen
// Created for ESP32 Sui Wallet

#include "ui.h"

lv_obj_t * ui_Screen5 = NULL;
lv_obj_t * ui_LabelStepsTitle = NULL;
lv_obj_t * ui_LabelStepCount = NULL;
lv_obj_t * ui_LabelStepsUnit = NULL;
lv_obj_t * ui_ButtonResetSteps = NULL;
lv_obj_t * ui_LabelReset = NULL;
lv_obj_t * ui_Arc_Steps = NULL;

// External functions
extern void resetStepCounter(void);
extern int stepCount;

// Event functions
void ui_event_ButtonResetSteps(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) {
        // Reset step counter
        resetStepCounter();

        // Update UI immediately
        lv_label_set_text(ui_LabelStepCount, "0");
        lv_arc_set_value(ui_Arc_Steps, 0);
    }
}

void ui_event_Screen5(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, &ui_Screen1_screen_init);
    }
}

// Build functions
void ui_Screen5_screen_init(void)
{
    ui_Screen5 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen5, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    ui_LabelStepsTitle = lv_label_create(ui_Screen5);
    lv_obj_set_width(ui_LabelStepsTitle, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelStepsTitle, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelStepsTitle, 0);
    lv_obj_set_y(ui_LabelStepsTitle, -100);
    lv_obj_set_align(ui_LabelStepsTitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelStepsTitle, "Steps Today");
    lv_obj_set_style_text_font(ui_LabelStepsTitle, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Arc progress (0-1000 steps)
    ui_Arc_Steps = lv_arc_create(ui_Screen5);
    lv_obj_set_width(ui_Arc_Steps, 160);
    lv_obj_set_height(ui_Arc_Steps, 160);
    lv_obj_set_x(ui_Arc_Steps, 0);
    lv_obj_set_y(ui_Arc_Steps, -10);
    lv_obj_set_align(ui_Arc_Steps, LV_ALIGN_CENTER);
    lv_arc_set_range(ui_Arc_Steps, 0, 1000);  // Max 1000 steps display
    lv_arc_set_value(ui_Arc_Steps, 0);
    lv_obj_clear_flag(ui_Arc_Steps, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(ui_Arc_Steps, lv_color_hex(0x00FF00), LV_PART_INDICATOR);

    // Step count number (big)
    ui_LabelStepCount = lv_label_create(ui_Screen5);
    lv_obj_set_width(ui_LabelStepCount, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelStepCount, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelStepCount, 0);
    lv_obj_set_y(ui_LabelStepCount, -10);
    lv_obj_set_align(ui_LabelStepCount, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelStepCount, "0");
    lv_obj_set_style_text_font(ui_LabelStepCount, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelStepCount, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);

    // "steps" label
    ui_LabelStepsUnit = lv_label_create(ui_Screen5);
    lv_obj_set_width(ui_LabelStepsUnit, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelStepsUnit, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelStepsUnit, 0);
    lv_obj_set_y(ui_LabelStepsUnit, 60);
    lv_obj_set_align(ui_LabelStepsUnit, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelStepsUnit, "steps");
    lv_obj_set_style_text_font(ui_LabelStepsUnit, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Reset button
    ui_ButtonResetSteps = lv_btn_create(ui_Screen5);
    lv_obj_set_width(ui_ButtonResetSteps, 80);
    lv_obj_set_height(ui_ButtonResetSteps, 30);
    lv_obj_set_x(ui_ButtonResetSteps, 0);
    lv_obj_set_y(ui_ButtonResetSteps, 90);
    lv_obj_set_align(ui_ButtonResetSteps, LV_ALIGN_CENTER);

    ui_LabelReset = lv_label_create(ui_ButtonResetSteps);
    lv_obj_set_align(ui_LabelReset, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelReset, "Reset");

    // Add event callbacks
    lv_obj_add_event_cb(ui_Screen5, ui_event_Screen5, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonResetSteps, ui_event_ButtonResetSteps, LV_EVENT_ALL, NULL);
}

void ui_Screen5_screen_destroy(void)
{
    if(ui_Screen5) lv_obj_del(ui_Screen5);

    // NULL screen variables
    ui_Screen5 = NULL;
    ui_LabelStepsTitle = NULL;
    ui_LabelStepCount = NULL;
    ui_LabelStepsUnit = NULL;
    ui_ButtonResetSteps = NULL;
    ui_LabelReset = NULL;
    ui_Arc_Steps = NULL;
}
