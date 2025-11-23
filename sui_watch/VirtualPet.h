/**
 * Virtual Pet for ESP32 Watch
 * Interactive pet that lives on your wrist!
 */

#ifndef VIRTUAL_PET_H
#define VIRTUAL_PET_H

#include <Arduino.h>
#include <lvgl.h>

// Pet evolution levels
enum PetLevel {
    LEVEL_EGG = 0,
    LEVEL_BABY = 1,
    LEVEL_TEEN = 2,
    LEVEL_ADULT = 3,
    LEVEL_MASTER = 4
};

// Pet moods
enum PetMood {
    MOOD_HAPPY,
    MOOD_NORMAL,
    MOOD_SAD,
    MOOD_HUNGRY,
    MOOD_SLEEPY,
    MOOD_PLAYFUL
};

// Pet animations
enum PetAnimation {
    ANIM_IDLE,
    ANIM_WALK,
    ANIM_EAT,
    ANIM_PLAY,
    ANIM_SLEEP,
    ANIM_EVOLVE,
    ANIM_HAPPY,
    ANIM_SAD
};

class VirtualPet {
public:
    VirtualPet();

    // Core functions
    void init(const String& name);
    void update(unsigned long currentTime);
    void feed();  // Use food to feed pet (+10 evolution points)
    void play();  // Use energy to play with pet (+5 evolution points)
    void sleep();

    // Resource management (earned from walking)
    void addFood(int amount);
    void addEnergy(int amount);
    bool canFeed();  // Check if can feed (has food + cooldown passed)
    bool canPlay();  // Check if can play (has energy + cooldown passed)

    // Evolution
    bool checkEvolution();
    void evolve();

    // Status
    PetMood getMood();
    bool needsAttention();
    String getStatusText();

    // Getters
    String getName() { return _name; }
    PetLevel getLevel() { return _level; }
    int getHappiness() { return _happiness; }
    int getHunger() { return _hunger; }
    int getHealth() { return _health; }
    int getExperience() { return _experience; }
    unsigned long getTotalStepsFed() { return _totalStepsFed; }
    int getFood() { return _food; }
    int getEnergy() { return _energy; }
    bool isEating() { return _isEating; }
    bool isPlaying() { return _isPlaying; }
    bool isBusy() { return _isEating || _isPlaying; }

    // Display
    void draw(lv_obj_t* parent);
    void animate(PetAnimation anim);

    // Blockchain sync
    String toJSON();
    void fromJSON(const String& json);

    // Animation control (public for UI access)
    void updateAnimation();
    const lv_img_dsc_t* getPetImage();

private:
    // Basic info
    String _name;
    PetLevel _level;

    // Stats (0-100)
    int _happiness;
    int _hunger;
    int _health;

    // Progress
    int _experience;
    unsigned long _totalStepsFed;

    // Resources (earned from walking)
    int _food;        // Used for feeding
    int _energy;      // Used for playing

    // Timestamps
    unsigned long _birthTime;
    unsigned long _lastFedTime;
    unsigned long _lastPlayTime;
    unsigned long _lastUpdateTime;

    // Appearance
    String _color;
    String _accessory;

    // LVGL objects
    lv_obj_t* _petImage;
    lv_obj_t* _statusBar;
    lv_obj_t* _moodIcon;
    lv_anim_t _currentAnim;

    // Animation frames (for image animation)
    const lv_img_dsc_t** _currentImageFrames;
    int _frameCount;
    int _currentFrame;
    unsigned long _lastFrameTime;

    // Animation states
    bool _isEating;
    unsigned long _eatAnimationStartTime;
    const unsigned long EAT_ANIMATION_DURATION = 20000;  // 20 seconds

    bool _isPlaying;
    unsigned long _playAnimationStartTime;
    const unsigned long PLAY_ANIMATION_DURATION = 20000;  // 20 seconds

    // Internal methods
    void updateStats(unsigned long deltaTime);
    void updateMood();
    const char* getMoodIcon();
};

// ============================================
// Pet Sprites (Simple ASCII art for now)
// ============================================

// Sprite declarations (defined in VirtualPet.cpp)
extern const char* EGG_IDLE[];
extern const char* BABY_IDLE[];
extern const char* BABY_HAPPY[];
extern const char* BABY_SAD[];
extern const char* TEEN_IDLE[];
extern const char* TEEN_HAPPY[];
extern const char* ADULT_IDLE[];
extern const char* ADULT_HAPPY[];
extern const char* MASTER_IDLE[];

#endif // VIRTUAL_PET_H