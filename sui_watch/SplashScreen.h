/**
 * Splash Screen with Blinking Text
 * Shows "Walmagotchi" text with blinking animation
 */

#ifndef SPLASH_SCREEN_H
#define SPLASH_SCREEN_H

#include <lvgl.h>

class SplashScreen {
private:
    lv_obj_t* screen;
    lv_obj_t* label;
    lv_timer_t* blinkTimer;
    bool isVisible;
    int blinkCount;
    unsigned long showTime;
    uint32_t duration;

    static SplashScreen* instance;
    static void blinkCallback(lv_timer_t* timer);

public:
    SplashScreen();

    // Show splash screen with blinking animation
    void show(uint32_t duration = 3000);  // Default 3 seconds

    // Hide splash screen
    void hide();

    // Check if showing
    bool isShowing() { return isVisible; }

    // Check if duration has elapsed (call in loop)
    bool shouldTransition();
};

#endif
