/**
 * Pet Sprite Images
 * Animation frames for virtual pet
 */

#ifndef PET_SPRITES_H
#define PET_SPRITES_H

#include <lvgl.h>

// Forward declarations - actual definitions in .c files
#ifdef __cplusplus
extern "C" {
#endif

// Pet idle animation frames (generated from assets/idle/*.png)
extern const lv_img_dsc_t pet_idle_frame1;  // 2.png
extern const lv_img_dsc_t pet_idle_frame2;  // 3.png
extern const lv_img_dsc_t pet_idle_frame3;  // 4.png

// Pet eat animation frames (generated from assets/eat/*.png)
extern const lv_img_dsc_t eat_frame1;
extern const lv_img_dsc_t eat_frame2;
extern const lv_img_dsc_t eat_frame3;
extern const lv_img_dsc_t eat_frame4;

// Pet play animation frames (generated from assets/play/*.png)
extern const lv_img_dsc_t play_frame1;
extern const lv_img_dsc_t play_frame2;
extern const lv_img_dsc_t play_frame3;
extern const lv_img_dsc_t play_frame4;

#ifdef __cplusplus
}
#endif

// Animation frame arrays
static const lv_img_dsc_t* PET_IDLE_FRAMES[] = {
    &pet_idle_frame1,
    &pet_idle_frame2,
    &pet_idle_frame3
};

static const lv_img_dsc_t* PET_EAT_FRAMES[] = {
    &eat_frame1,
    &eat_frame2,
    &eat_frame3,
    &eat_frame4
};

static const lv_img_dsc_t* PET_PLAY_FRAMES[] = {
    &play_frame1,
    &play_frame2,
    &play_frame3,
    &play_frame4
};

#define PET_IDLE_FRAME_COUNT 3
#define PET_EAT_FRAME_COUNT 4
#define PET_PLAY_FRAME_COUNT 4

#endif // PET_SPRITES_H
