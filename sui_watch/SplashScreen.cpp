/**
 * Splash Screen Implementation
 */

#include <Arduino.h>
#include "SplashScreen.h"

// Static instance for callbacks
SplashScreen* SplashScreen::instance = nullptr;

SplashScreen::SplashScreen() {
    screen = nullptr;
    label = nullptr;
    blinkTimer = nullptr;
    isVisible = false;
    blinkCount = 0;
    showTime = 0;
    duration = 0;
    instance = this;
}

void SplashScreen::show(uint32_t dur) {
    if (isVisible) return;

    Serial.println("[SPLASH] Creating splash screen...");

    this->duration = dur;
    this->showTime = millis();

    // Create full-screen splash
    screen = lv_obj_create(nullptr);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Dark background
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);

    // Create "Walmagotchi" label
    label = lv_label_create(screen);
    lv_label_set_text(label, "Walmagotchi");
    lv_obj_center(label);

    // Style the label
    lv_obj_set_style_text_color(label, lv_color_hex(0x00ADB5), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);

    // Load the splash screen
    lv_scr_load(screen);

    isVisible = true;
    blinkCount = 0;

    // Start blinking animation (500ms interval)
    blinkTimer = lv_timer_create(blinkCallback, 500, nullptr);

    Serial.println("[SPLASH] Splash screen shown");
}

void SplashScreen::hide() {
    if (!isVisible) return;

    Serial.println("[SPLASH] Hiding splash screen...");

    // Stop blink timer
    if (blinkTimer) {
        lv_timer_del(blinkTimer);
        blinkTimer = nullptr;
    }

    // Delete screen objects
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
        label = nullptr;
    }

    isVisible = false;

    Serial.println("[SPLASH] Splash screen hidden");
}

bool SplashScreen::shouldTransition() {
    if (!isVisible) return false;

    unsigned long elapsed = millis() - showTime;
    return elapsed >= duration;
}

void SplashScreen::blinkCallback(lv_timer_t* timer) {
    if (!instance || !instance->label) return;

    // Toggle visibility for blinking effect
    if (instance->blinkCount % 2 == 0) {
        lv_obj_add_flag(instance->label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(instance->label, LV_OBJ_FLAG_HIDDEN);
    }

    instance->blinkCount++;
}
