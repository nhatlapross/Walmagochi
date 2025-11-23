/**
 * Virtual Pet Implementation
 */

#include "VirtualPet.h"
#include "pet_sprites.h"
#include <ArduinoJson.h>

// ============================================
// Pet Sprite Definitions
// ============================================

// Egg sprites
const char* EGG_IDLE[] = {
    "  ___  ",
    " /   \\ ",
    "|     |",
    " \\___/ ",
};

// Baby pet sprites
const char* BABY_IDLE[] = {
    " ^_^ ",
    "(o.o)",
    " > < ",
};

const char* BABY_HAPPY[] = {
    " ^▽^ ",
    "\\(^o^)/",
    "  > <  ",
};

const char* BABY_SAD[] = {
    " ;_; ",
    "(T.T)",
    " > < ",
};

// Teen pet sprites
const char* TEEN_IDLE[] = {
    "  ___  ",
    " (^.^) ",
    "d|   |b",
    "  | |  ",
};

const char* TEEN_HAPPY[] = {
    "  ___  ",
    " (^▽^) ",
    "d| ♥ |b",
    "  | |  ",
};

// Adult pet sprites
const char* ADULT_IDLE[] = {
    "   ___   ",
    "  (o.o)  ",
    " d|   |b ",
    "  |   |  ",
    "  d   b  ",
};

const char* ADULT_HAPPY[] = {
    "   ___   ",
    "  (^▽^)  ",
    " d| ♥ |b ",
    "  |   |  ",
    "  d   b  ",
};

// Master pet sprites
const char* MASTER_IDLE[] = {
    "    👑    ",
    "   ___   ",
    "  (◕.◕)  ",
    " d|⭐|b ",
    "  |   |  ",
    "  d   b  ",
};

VirtualPet::VirtualPet() {
    _level = LEVEL_EGG;
    _happiness = 50;
    _hunger = 50;
    _health = 100;
    _experience = 0;
    _totalStepsFed = 0;
    _food = 5;         // Start with 5 food
    _energy = 5;       // Start with 5 energy
    _birthTime = millis();
    _lastFedTime = millis();  // Start timer from birth
    _lastPlayTime = millis(); // Start timer from birth
    _lastUpdateTime = millis();
    _color = "blue";
    _accessory = "none";
    _petImage = nullptr;
    _statusBar = nullptr;
    _moodIcon = nullptr;
    _currentImageFrames = PET_IDLE_FRAMES;  // Start with idle animation
    _frameCount = PET_IDLE_FRAME_COUNT;
    _currentFrame = 0;
    _lastFrameTime = millis();
    _isEating = false;
    _eatAnimationStartTime = 0;
    _isPlaying = false;
    _playAnimationStartTime = 0;
}

void VirtualPet::init(const String& name) {
    _name = name;
    Serial.println("🥚 Pet born: " + _name);
}

void VirtualPet::update(unsigned long currentTime) {
    unsigned long deltaTime = currentTime - _lastUpdateTime;
    if (deltaTime < 1000) return;  // Update every second

    _lastUpdateTime = currentTime;
    updateStats(deltaTime);
    updateMood();

    // Check if needs attention
    if (needsAttention()) {
        Serial.println("⚠️ Pet needs attention!");
    }
}

void VirtualPet::feed() {
    if (!canFeed()) {
        Serial.println("Cannot feed: no food or cooldown active");
        return;
    }

    if (_hunger >= 100) {
        Serial.println("Pet is full!");
        return;
    }

    // Use 1 food to feed
    _food--;

    // Increase hunger (significant) and happiness (small bonus)
    int hungerIncrease = 25;  // Each food gives 25 hunger (4 feeds to fill from 0)
    int happinessBonus = 5;   // Small happiness bonus from eating

    _hunger = min(100, _hunger + hungerIncrease);
    _happiness = min(100, _happiness + happinessBonus);

    // Add evolution points
    _experience += 10;
    _totalStepsFed += 100; // Track equivalent steps
    _lastFedTime = millis();

    Serial.printf("🍔 Fed pet! Hunger: %d (+%d), Happiness: %d (+%d), Food left: %d, XP: +10\n",
                  _hunger, hungerIncrease, _happiness, happinessBonus, _food);

    // Check evolution
    if (checkEvolution()) {
        evolve();
    }

    // Play feeding animation
    animate(ANIM_EAT);
}

void VirtualPet::play() {
    if (!canPlay()) {
        Serial.println("Cannot play: no energy or cooldown active");
        return;
    }

    // Use 1 energy to play
    _energy--;

    // Increase happiness significantly from playing
    int happinessIncrease = 15;  // Each play gives 15 happiness (7 plays to fill from 0)

    _happiness = min(100, _happiness + happinessIncrease);

    // Add evolution points
    _experience += 5;
    _lastPlayTime = millis();

    Serial.printf("🎮 Played with pet! Happiness: %d (+%d), Energy left: %d, XP: +5\n",
                  _happiness, happinessIncrease, _energy);
    animate(ANIM_PLAY);
}

void VirtualPet::sleep() {
    _health = min(100, _health + 10);
    Serial.println("😴 Pet is sleeping...");
    animate(ANIM_SLEEP);
}

bool VirtualPet::checkEvolution() {
    if (_level == LEVEL_EGG && _totalStepsFed >= 1000) return true;
    if (_level == LEVEL_BABY && _totalStepsFed >= 10000) return true;
    if (_level == LEVEL_TEEN && _totalStepsFed >= 50000) return true;
    if (_level == LEVEL_ADULT && _totalStepsFed >= 100000) return true;
    return false;
}

void VirtualPet::evolve() {
    PetLevel oldLevel = _level;

    if (_totalStepsFed >= 100000 && _level < LEVEL_MASTER) {
        _level = LEVEL_MASTER;
    } else if (_totalStepsFed >= 50000 && _level < LEVEL_ADULT) {
        _level = LEVEL_ADULT;
    } else if (_totalStepsFed >= 10000 && _level < LEVEL_TEEN) {
        _level = LEVEL_TEEN;
    } else if (_totalStepsFed >= 1000 && _level < LEVEL_BABY) {
        _level = LEVEL_BABY;
    }

    if (oldLevel != _level) {
        Serial.printf("🎉 Pet evolved from level %d to %d!\n", oldLevel, _level);
        animate(ANIM_EVOLVE);

        // Restore health on evolution
        _health = 100;
        _happiness = min(100, _happiness + 30);
    }
}

PetMood VirtualPet::getMood() {
    if (_happiness > 80) return MOOD_HAPPY;
    if (_happiness < 30) return MOOD_SAD;
    if (_hunger < 30) return MOOD_HUNGRY;

    unsigned long timeSincePlay = millis() - _lastPlayTime;
    if (timeSincePlay > 7200000) return MOOD_SLEEPY;  // 2 hours
    if (timeSincePlay < 600000) return MOOD_PLAYFUL;  // 10 minutes

    return MOOD_NORMAL;
}

bool VirtualPet::needsAttention() {
    return _happiness < 30 || _hunger < 30 || _health < 50;
}

String VirtualPet::getStatusText() {
    String status = _name + " (Lv." + String(_level) + ")\n";
    status += "😊 " + String(_happiness) + "% ";
    status += "🍔 " + String(_hunger) + "% ";
    status += "❤️ " + String(_health) + "%\n";

    // Check if performing an animation action
    if (_isEating) {
        status += "Status: Eating... 🍽️";
        return status;
    }

    if (_isPlaying) {
        status += "Status: Playing... 🎮";
        return status;
    }

    // Normal mood display
    PetMood mood = getMood();
    switch (mood) {
        case MOOD_HAPPY: status += "Mood: Happy 😄"; break;
        case MOOD_SAD: status += "Mood: Sad 😢"; break;
        case MOOD_HUNGRY: status += "Mood: Hungry 🍔"; break;
        case MOOD_SLEEPY: status += "Mood: Sleepy 😴"; break;
        case MOOD_PLAYFUL: status += "Mood: Playful 🎮"; break;
        default: status += "Mood: Normal 😊"; break;
    }

    return status;
}

void VirtualPet::draw(lv_obj_t* parent) {
    // Create pet image if not exists
    if (!_petImage) {
        _petImage = lv_img_create(parent);  // Use lv_img instead of lv_label
        lv_obj_align(_petImage, LV_ALIGN_CENTER, 0, -20);
    }

    // Update animation frame
    updateAnimation();

    // Update pet image
    const lv_img_dsc_t* currentImage = getPetImage();
    if (currentImage) {
        lv_img_set_src(_petImage, currentImage);
    }

    // Create status bar if not exists
    if (!_statusBar) {
        _statusBar = lv_label_create(parent);
        lv_obj_set_style_text_color(_statusBar, lv_color_hex(0xCCCCCC), 0);  // Light gray
        lv_obj_align(_statusBar, LV_ALIGN_BOTTOM_MID, 0, -10);
    }

    // Update status with resources
    char status[80];
    snprintf(status, sizeof(status), "H:%d F:%d HP:%d | Food:%d Nrg:%d",
             _happiness, _hunger, _health, _food, _energy);
    lv_label_set_text(_statusBar, status);

    // Create mood icon if not exists
    if (!_moodIcon) {
        _moodIcon = lv_label_create(parent);
        lv_obj_set_style_text_color(_moodIcon, lv_color_hex(0xFFFF00), 0);  // Yellow
        lv_obj_align(_moodIcon, LV_ALIGN_TOP_RIGHT, -10, 10);
    }

    // Update mood icon
    lv_label_set_text(_moodIcon, getMoodIcon());
}

void VirtualPet::animate(PetAnimation anim) {
    // Simple animation by changing frames
    // In real implementation, use lv_anim for smooth animations

    switch (anim) {
        case ANIM_EAT:
            Serial.println("🍽️ [EAT] Starting eating animation (20s)");
            // Switch to eating animation
            _isEating = true;
            _eatAnimationStartTime = millis();
            _currentImageFrames = PET_EAT_FRAMES;
            _frameCount = PET_EAT_FRAME_COUNT;
            _currentFrame = 0;
            break;
        case ANIM_PLAY:
            Serial.println("🎮 [PLAY] Starting play animation (20s)");
            // Switch to play animation
            _isPlaying = true;
            _playAnimationStartTime = millis();
            _currentImageFrames = PET_PLAY_FRAMES;
            _frameCount = PET_PLAY_FRAME_COUNT;
            _currentFrame = 0;
            break;
        case ANIM_SLEEP:
            Serial.println("[SLEEP] *zzz...*");
            break;
        case ANIM_EVOLVE:
            Serial.println("[EVOLVE] *sparkle sparkle*");
            break;
        case ANIM_HAPPY:
            Serial.println("[HAPPY] *joy joy*");
            break;
        case ANIM_SAD:
            Serial.println("[SAD] *sniffle*");
            break;
        default:
            break;
    }
}

String VirtualPet::toJSON() {
    JsonDocument doc;
    doc["name"] = _name;
    doc["level"] = _level;
    doc["happiness"] = _happiness;
    doc["hunger"] = _hunger;
    doc["health"] = _health;
    doc["experience"] = _experience;
    doc["totalStepsFed"] = _totalStepsFed;
    doc["color"] = _color;
    doc["accessory"] = _accessory;

    String json;
    serializeJson(doc, json);
    return json;
}

void VirtualPet::fromJSON(const String& json) {
    JsonDocument doc;
    deserializeJson(doc, json);

    _name = doc["name"].as<String>();
    _level = (PetLevel)doc["level"].as<int>();
    _happiness = doc["happiness"];
    _hunger = doc["hunger"];
    _health = doc["health"];
    _experience = doc["experience"];
    _totalStepsFed = doc["totalStepsFed"];
    _color = doc["color"].as<String>();
    _accessory = doc["accessory"].as<String>();
}

// Private methods

void VirtualPet::updateStats(unsigned long deltaTime) {
    // Hunger decreases over time (faster than before)
    // Decreases by 1 point every 5 minutes
    unsigned long timeSinceFed = millis() - _lastFedTime;
    if (timeSinceFed > 300000 && _hunger > 0) {  // 5 minutes
        int hungerDecrease = (timeSinceFed / 300000);  // 1 point per 5 min
        _hunger = max(0, _hunger - hungerDecrease);
        _lastFedTime = millis();  // Reset timer to prevent instant decrease
    }

    // Happiness decreases if not played with
    // Decreases by 1 point every 10 minutes
    unsigned long timeSincePlay = millis() - _lastPlayTime;
    if (timeSincePlay > 600000 && _happiness > 0) {  // 10 minutes
        int happinessDecrease = (timeSincePlay / 600000);  // 1 point per 10 min
        _happiness = max(0, _happiness - happinessDecrease);
        _lastPlayTime = millis();  // Reset timer
    }

    // Health affected by hunger and happiness
    if (_hunger < 20 || _happiness < 20) {
        // Pet is unhappy or hungry - health decreases
        if (_health > 0) _health = max(0, _health - 1);
    } else if (_hunger > 60 && _happiness > 60 && _health < 100) {
        // Pet is well fed and happy - health increases slowly
        _health = min(100, _health + 1);
    }
}

void VirtualPet::updateMood() {
    // Mood affects animation
    PetMood mood = getMood();
    if (mood == MOOD_HAPPY && random(100) < 10) {
        animate(ANIM_HAPPY);
    } else if (mood == MOOD_SAD && random(100) < 10) {
        animate(ANIM_SAD);
    }
}


const char* VirtualPet::getMoodIcon() {
    PetMood mood = getMood();
    switch (mood) {
        case MOOD_HAPPY: return ":)";
        case MOOD_SAD: return ":(";
        case MOOD_HUNGRY: return "!F";
        case MOOD_SLEEPY: return "zz";
        case MOOD_PLAYFUL: return "^^";
        default: return ":)";
    }
}

// ============================================
// Resource Management Methods
// ============================================

void VirtualPet::addFood(int amount) {
    _food = min(999, _food + amount);  // Max 999 food
    Serial.printf("🍖 +%d food! Total: %d\n", amount, _food);
}

void VirtualPet::addEnergy(int amount) {
    _energy = min(999, _energy + amount);  // Max 999 energy
    Serial.printf("⚡ +%d energy! Total: %d\n", amount, _energy);
}

bool VirtualPet::canFeed() {
    // Need at least 1 food
    if (_food < 1) return false;

    // Check cooldown based on level (in milliseconds)
    unsigned long cooldown;
    switch (_level) {
        case LEVEL_EGG:    cooldown = 60000;   break;  // 1 minute
        case LEVEL_BABY:   cooldown = 120000;  break;  // 2 minutes
        case LEVEL_TEEN:   cooldown = 180000;  break;  // 3 minutes
        case LEVEL_ADULT:  cooldown = 300000;  break;  // 5 minutes
        case LEVEL_MASTER: cooldown = 600000;  break;  // 10 minutes
        default:           cooldown = 120000;  break;
    }

    unsigned long timeSinceLastFeed = millis() - _lastFedTime;
    return timeSinceLastFeed >= cooldown;
}

bool VirtualPet::canPlay() {
    // Need at least 1 energy
    if (_energy < 1) return false;

    // Check cooldown based on level (in milliseconds)
    unsigned long cooldown;
    switch (_level) {
        case LEVEL_EGG:    cooldown = 30000;   break;  // 30 seconds
        case LEVEL_BABY:   cooldown = 60000;   break;  // 1 minute
        case LEVEL_TEEN:   cooldown = 90000;   break;  // 1.5 minutes
        case LEVEL_ADULT:  cooldown = 120000;  break;  // 2 minutes
        case LEVEL_MASTER: cooldown = 180000;  break;  // 3 minutes
        default:           cooldown = 60000;   break;
    }

    unsigned long timeSinceLastPlay = millis() - _lastPlayTime;
    return timeSinceLastPlay >= cooldown;
}

// ============================================
// Image Animation Methods
// ============================================

void VirtualPet::updateAnimation() {
    unsigned long currentTime = millis();

    // Check if eating animation should end
    if (_isEating && (currentTime - _eatAnimationStartTime) >= EAT_ANIMATION_DURATION) {
        // Return to idle animation
        _isEating = false;
        _currentImageFrames = PET_IDLE_FRAMES;
        _frameCount = PET_IDLE_FRAME_COUNT;
        _currentFrame = 0;
        Serial.println("🍽️ Finished eating animation");
    }

    // Check if playing animation should end
    if (_isPlaying && (currentTime - _playAnimationStartTime) >= PLAY_ANIMATION_DURATION) {
        // Return to idle animation
        _isPlaying = false;
        _currentImageFrames = PET_IDLE_FRAMES;
        _frameCount = PET_IDLE_FRAME_COUNT;
        _currentFrame = 0;
        Serial.println("🎮 Finished playing animation");
    }

    // Change frame every 200ms (5 FPS animation)
    if (currentTime - _lastFrameTime > 200) {
        _lastFrameTime = currentTime;
        _currentFrame = (_currentFrame + 1) % _frameCount;
    }
}

const lv_img_dsc_t* VirtualPet::getPetImage() {
    // Return current animation frame
    if (_currentImageFrames && _frameCount > 0) {
        return _currentImageFrames[_currentFrame];
    }

    // Fallback to first frame
    return &pet_idle_frame1;
}