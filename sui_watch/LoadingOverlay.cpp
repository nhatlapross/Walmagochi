/**
 * Loading Overlay Implementation
 */

#include <Arduino.h>
#include "LoadingOverlay.h"

LoadingOverlay::LoadingOverlay() {
    overlay = nullptr;
    spinner = nullptr;
    label = nullptr;
    isShowing = false;
}

void LoadingOverlay::show(const char* message) {
    if (isShowing) {
        updateMessage(message);
        return;
    }

    // Create full-screen overlay
    overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Semi-transparent dark background
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, 200, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);

    // Center container
    lv_obj_t* container = lv_obj_create(overlay);
    lv_obj_set_size(container, 200, 150);
    lv_obj_center(container);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_bg_opa(container, 255, 0);
    lv_obj_set_style_border_width(container, 2, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x00ADB5), 0);
    lv_obj_set_style_radius(container, 15, 0);

    // Create spinner
    spinner = lv_spinner_create(container, 1000, 60);
    lv_obj_set_size(spinner, 60, 60);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_arc_width(spinner, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x00ADB5), LV_PART_INDICATOR);

    // Create label
    label = lv_label_create(container);
    lv_label_set_text(label, message);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    // Bring to front
    lv_obj_move_foreground(overlay);

    isShowing = true;
    Serial.println("[LOADING] Overlay shown");
}

void LoadingOverlay::hide() {
    if (!isShowing || !overlay) {
        return;
    }

    lv_obj_del(overlay);
    overlay = nullptr;
    spinner = nullptr;
    label = nullptr;
    isShowing = false;

    Serial.println("[LOADING] Overlay hidden");
}

void LoadingOverlay::updateMessage(const char* message) {
    if (!isShowing || !label) {
        return;
    }

    lv_label_set_text(label, message);
    Serial.printf("[LOADING] Message updated: %s\n", message);
}
