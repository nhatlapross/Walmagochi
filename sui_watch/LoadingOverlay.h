/**
 * Loading Overlay for Blockchain Operations
 * Shows a loading screen while waiting for blockchain response
 */

#ifndef LOADING_OVERLAY_H
#define LOADING_OVERLAY_H

#include <lvgl.h>

class LoadingOverlay {
private:
    lv_obj_t* overlay;
    lv_obj_t* spinner;
    lv_obj_t* label;
    bool isShowing;

public:
    LoadingOverlay();

    // Show loading with message
    void show(const char* message = "Processing...");

    // Hide loading
    void hide();

    // Update message
    void updateMessage(const char* message);

    // Check if showing
    bool isVisible() { return isShowing; }
};

#endif
