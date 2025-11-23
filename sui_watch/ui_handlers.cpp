/**
 * UI Event Handlers for SquareLine Studio Screens
 */

#include <Arduino.h>
#include <lvgl.h>
#include "ui.h"
#include "VirtualPet.h"
#include "TrustOracleClient.h"
#include "LoadingOverlay.h"

// External references
extern VirtualPet virtualPet;
extern int stepCount;
extern TrustOracleClient* oracleClient;
extern const char* DEVICE_WALLET_ADDRESS;
extern String suiBalance;
extern String petObjectId;  // Pet NFT Object ID
extern void fetchSuiBalance();  // Function to manually trigger balance fetch

// Pending steps for claim
int pendingSteps = 0;

// Loading overlay instance (accessible from other files via extern)
LoadingOverlay loadingOverlay;

// ============================================
// Screen 1: Pet Display
// ============================================

void updateScreen1PetUI() {
    // Update pet image animation (handled by VirtualPet)
    virtualPet.updateAnimation();
    const lv_img_dsc_t* petImg = virtualPet.getPetImage();
    if (petImg) {
        lv_img_set_src(ui_Image2, petImg);
    }

    // Update pet level/maturity
    char levelBuf[32];
    const char* levelNames[] = {"Egg", "Baby", "Teen", "Adult", "Master"};
    snprintf(levelBuf, sizeof(levelBuf), "Walrus %s", levelNames[virtualPet.getLevel()]);
    lv_label_set_text(ui_Label6, levelBuf);

    // Update pet NFT address (shortened format)
    if (petObjectId.length() > 10) {
        char shortAddr[20];
        // Show first 6 and last 4 characters: 0x1234...5678
        snprintf(shortAddr, sizeof(shortAddr), "%.6s...%.4s",
                 petObjectId.c_str(),
                 petObjectId.c_str() + petObjectId.length() - 4);
        lv_label_set_text(ui_txtPetAddress, shortAddr);
    } else {
        lv_label_set_text(ui_txtPetAddress, "Not registered");
    }

    // Update status label dynamically
    if (virtualPet.isEating()) {
        lv_label_set_text(ui_status, "Eating...");
    } else if (virtualPet.isPlaying()) {
        lv_label_set_text(ui_status, "Playing...");
    } else {
        // Show mood when not busy
        if (virtualPet.getHappiness() > 70) {
            lv_label_set_text(ui_status, "Happy");
        } else if (virtualPet.getHappiness() < 30) {
            lv_label_set_text(ui_status, "Sad");
        } else if (virtualPet.getHunger() < 30) {
            lv_label_set_text(ui_status, "Hungry");
        } else {
            lv_label_set_text(ui_status, "Normal");
        }
    }

    // Update happiness bar
    lv_bar_set_value(ui_Bar1, virtualPet.getHappiness(), LV_ANIM_OFF);

    // Update hunger bar
    lv_bar_set_value(ui_Bar2, virtualPet.getHunger(), LV_ANIM_OFF);
}

// ============================================
// Screen 2: Feed & Play
// ============================================

void updateScreen2ResourcesUI() {
    // Update food count
    char foodBuf[16];
    snprintf(foodBuf, sizeof(foodBuf), "%d", virtualPet.getFood());
    lv_label_set_text(ui_txtFood, foodBuf);

    // Update energy count
    char energyBuf[16];
    snprintf(energyBuf, sizeof(energyBuf), "%d", virtualPet.getEnergy());
    lv_label_set_text(ui_txtEnery, energyBuf);

    // Disable ALL buttons when pet is busy (eating or playing)
    if (virtualPet.isBusy()) {
        lv_obj_add_state(ui_btnFeed, LV_STATE_DISABLED);
        lv_obj_add_state(ui_btnPlay, LV_STATE_DISABLED);
        return;  // Don't check other conditions
    }

    // Enable/disable feed button based on resources and cooldown
    if (virtualPet.canFeed()) {
        lv_obj_clear_state(ui_btnFeed, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ui_btnFeed, LV_STATE_DISABLED);
    }

    // Enable/disable play button based on resources and cooldown
    if (virtualPet.canPlay()) {
        lv_obj_clear_state(ui_btnPlay, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ui_btnPlay, LV_STATE_DISABLED);
    }
}

void onFeedButtonClicked(lv_event_t* e) {
    Serial.println("[FEED] Feed button clicked!");

    // Debug: Check oracle client status
    Serial.printf("[FEED] Oracle client: %s\n", oracleClient ? "exists" : "NULL");
    if (oracleClient) {
        Serial.printf("[FEED] Connected: %s\n", oracleClient->isConnected() ? "YES" : "NO");
        Serial.printf("[FEED] Authenticated: %s\n", oracleClient->isAuthenticated() ? "YES" : "NO");
    }

    // Block if pet is busy with another action
    if (virtualPet.isBusy()) {
        Serial.println("[FEED] Pet is busy! Wait for current action to finish.");
        return;
    }

    // Block if loading is showing
    if (loadingOverlay.isVisible()) {
        Serial.println("[FEED] Loading overlay is showing! Please wait.");
        return;
    }

    // Debug: Check pet status
    Serial.printf("[FEED] Pet food: %d\n", virtualPet.getFood());
    Serial.printf("[FEED] Can feed: %s\n", virtualPet.canFeed() ? "YES" : "NO");

    if (virtualPet.canFeed()) {
        // Show loading overlay
        if (oracleClient && oracleClient->isAuthenticated()) {
            loadingOverlay.show("Feeding on blockchain...");
        }

        Serial.println("[FEED] Feeding pet locally...");
        virtualPet.feed();
        updateScreen2ResourcesUI();
        updateScreen1PetUI();

        // Sync with blockchain if connected
        if (oracleClient && oracleClient->isAuthenticated()) {
            Serial.println("[FEED] Syncing feed action with blockchain...");
            bool sent = oracleClient->feedPet();
            Serial.printf("[FEED] Message sent: %s\n", sent ? "SUCCESS" : "FAILED");

            if (!sent) {
                loadingOverlay.hide();
            }
            // Loading will be hidden when response is received in TrustOracleClient
        } else {
            Serial.println("[FEED] ⚠️ Not connected to blockchain");
        }
    } else {
        Serial.println("[FEED] Cannot feed: no food or cooldown active");
    }
}

void onPlayButtonClicked(lv_event_t* e) {
    Serial.println("[PLAY] Play button clicked!");

    // Debug: Check oracle client status
    Serial.printf("[PLAY] Oracle client: %s\n", oracleClient ? "exists" : "NULL");
    if (oracleClient) {
        Serial.printf("[PLAY] Connected: %s\n", oracleClient->isConnected() ? "YES" : "NO");
        Serial.printf("[PLAY] Authenticated: %s\n", oracleClient->isAuthenticated() ? "YES" : "NO");
    }

    // Block if pet is busy with another action
    if (virtualPet.isBusy()) {
        Serial.println("[PLAY] Pet is busy! Wait for current action to finish.");
        return;
    }

    // Block if loading is showing
    if (loadingOverlay.isVisible()) {
        Serial.println("[PLAY] Loading overlay is showing! Please wait.");
        return;
    }

    // Debug: Check pet status
    Serial.printf("[PLAY] Pet energy: %d\n", virtualPet.getEnergy());
    Serial.printf("[PLAY] Can play: %s\n", virtualPet.canPlay() ? "YES" : "NO");

    if (virtualPet.canPlay()) {
        // Show loading overlay
        if (oracleClient && oracleClient->isAuthenticated()) {
            loadingOverlay.show("Playing on blockchain...");
        }

        Serial.println("[PLAY] Playing with pet locally...");
        virtualPet.play();
        updateScreen2ResourcesUI();
        updateScreen1PetUI();

        // Sync with blockchain if connected
        if (oracleClient && oracleClient->isAuthenticated()) {
            Serial.println("[PLAY] Syncing play action with blockchain...");
            bool sent = oracleClient->playWithPet();
            Serial.printf("[PLAY] Message sent: %s\n", sent ? "SUCCESS" : "FAILED");

            if (!sent) {
                loadingOverlay.hide();
            }
            // Loading will be hidden when response is received in TrustOracleClient
        } else {
            Serial.println("[PLAY] ⚠️ Not connected to blockchain");
        }
    } else {
        Serial.println("[PLAY] Cannot play: no energy or cooldown active");
    }
}

// ============================================
// Screen 3: Steps & Claim
// ============================================

void updateScreen3StepsUI() {
    // Update step arc (0-1000 range)
    int displaySteps = min(stepCount, 1000);
    lv_arc_set_value(ui_arcStep, displaySteps / 10);  // Arc is 0-100, so divide by 10

    // Update step text
    char stepBuf[16];
    snprintf(stepBuf, sizeof(stepBuf), "%d", stepCount);
    lv_label_set_text(ui_txtStep, stepBuf);

    // Enable/disable claim button
    if (stepCount >= 100) {
        lv_obj_clear_state(ui_btnClaimCount, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ui_btnClaimCount, LV_STATE_DISABLED);
    }
}

void onClaimButtonClicked(lv_event_t* e) {
    Serial.println("[CLAIM] Claim button clicked!");

    // Debug: Check oracle client status
    Serial.printf("[CLAIM] Oracle client: %s\n", oracleClient ? "exists" : "NULL");
    if (oracleClient) {
        Serial.printf("[CLAIM] Connected: %s\n", oracleClient->isConnected() ? "YES" : "NO");
        Serial.printf("[CLAIM] Authenticated: %s\n", oracleClient->isAuthenticated() ? "YES" : "NO");
    }

    // Block if loading is showing
    if (loadingOverlay.isVisible()) {
        Serial.println("[CLAIM] Loading overlay is showing! Please wait.");
        return;
    }

    Serial.printf("[CLAIM] Current steps: %d\n", stepCount);

    if (stepCount >= 100) {
        // Show loading overlay
        if (oracleClient && oracleClient->isAuthenticated()) {
            loadingOverlay.show("Claiming resources...");
        }

        // Calculate resources
        int foodToAdd = stepCount / 100;      // Every 100 steps = 1 food
        int energyToAdd = (stepCount / 150) * 2;  // Every 150 steps = 2 energy

        // Add resources to pet locally
        virtualPet.addFood(foodToAdd);
        virtualPet.addEnergy(energyToAdd);

        Serial.printf("[CLAIM] Claimed locally: %d food, %d energy from %d steps\n",
                      foodToAdd, energyToAdd, stepCount);

        // Sync with blockchain if connected
        if (oracleClient && oracleClient->isAuthenticated()) {
            Serial.println("[CLAIM] Syncing claim with blockchain...");
            bool sent = oracleClient->claimResources(stepCount);
            Serial.printf("[CLAIM] Message sent: %s\n", sent ? "SUCCESS" : "FAILED");

            if (!sent) {
                loadingOverlay.hide();
            }
            // Loading will be hidden when response is received in TrustOracleClient
        } else {
            Serial.println("[CLAIM] ⚠️ Not connected to blockchain");
        }

        // Reset step count after claiming
        stepCount = 0;
        pendingSteps = 0;

        // Update all UIs
        updateScreen3StepsUI();
        updateScreen2ResourcesUI();
    } else {
        Serial.println("[CLAIM] Need at least 100 steps to claim");
    }
}

// ============================================
// Screen 4: Wallet & Sync
// ============================================

void updateScreen4WalletUI() {
    // Update wallet address - show shortened format
    if (DEVICE_WALLET_ADDRESS && strlen(DEVICE_WALLET_ADDRESS) > 10) {
        char shortAddr[20];
        // Show first 6 and last 4 characters: 0xb0bd...efe3
        snprintf(shortAddr, sizeof(shortAddr), "%.6s...%.4s",
                 DEVICE_WALLET_ADDRESS,
                 DEVICE_WALLET_ADDRESS + strlen(DEVICE_WALLET_ADDRESS) - 4);
        lv_label_set_text(ui_txtWallet, shortAddr);
    } else {
        lv_label_set_text(ui_txtWallet, "No wallet");
    }

    // Update SUI balance (from periodic fetch)
    String balanceText = suiBalance + " SUI";
    lv_label_set_text(ui_txtBalance, balanceText.c_str());

    // Update connection status
    if (oracleClient && oracleClient->isAuthenticated()) {
        lv_label_set_text(ui_txtConnect, "Connected");
    } else {
        lv_label_set_text(ui_txtConnect, "Disconnected");
    }
}

void onSyncButtonClicked(lv_event_t* e) {
    Serial.println("[SYNC] Sync button clicked!");

    if (oracleClient && oracleClient->isAuthenticated()) {
        // 1. Upload current pet state to blockchain
        String petJson = virtualPet.toJSON();
        bool success = oracleClient->syncPet(petJson);

        if (success) {
            Serial.println("[SYNC] ✓ Pet data synced to blockchain!");
            Serial.println("[SYNC] Pet state:");
            Serial.printf("  - Name: %s\n", virtualPet.getName().c_str());
            Serial.printf("  - Level: %d\n", virtualPet.getLevel());
            Serial.printf("  - Happiness: %d\n", virtualPet.getHappiness());
            Serial.printf("  - Hunger: %d\n", virtualPet.getHunger());
            Serial.printf("  - Health: %d\n", virtualPet.getHealth());
            Serial.printf("  - XP: %d\n", virtualPet.getExperience());
        } else {
            Serial.println("[SYNC] ✗ Pet sync failed!");
        }

        // 2. Fetch latest balance from Sui blockchain
        Serial.println("[SYNC] Fetching latest balance...");
        fetchSuiBalance();

        // 3. Update UI
        updateScreen4WalletUI();
    } else {
        Serial.println("[SYNC] Not connected to blockchain");
        Serial.println("[SYNC] Please check WiFi and server connection");
    }
}

// ============================================
// Setup Event Handlers
// ============================================

void setupUIHandlers() {
    // Screen 2 button handlers
    lv_obj_add_event_cb(ui_btnFeed, onFeedButtonClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnPlay, onPlayButtonClicked, LV_EVENT_CLICKED, NULL);

    // Screen 3 button handler
    lv_obj_add_event_cb(ui_btnClaimCount, onClaimButtonClicked, LV_EVENT_CLICKED, NULL);

    // Screen 4 button handler
    lv_obj_add_event_cb(ui_Button5, onSyncButtonClicked, LV_EVENT_CLICKED, NULL);

    Serial.println("[UI] Event handlers setup complete");
}
