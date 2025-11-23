# ESP32-S3 + GC9A01 LCD 1.28" + LVGL Setup Guide

## Hardware Specifications
- **Board**: ESP32-S3 Dev Module
- **Display**: GC9A01 1.28" Round LCD (240x240)
- **Touch**: CST816S capacitive touch controller
- **PSRAM**: 2MB (required for frame buffer)

## Pin Configuration

### LCD Pins (SPI)
```cpp
#define LCD_DC_PIN      8   // Data/Command
#define LCD_CS_PIN      9   // Chip Select
#define LCD_CLK_PIN     10  // SPI Clock
#define LCD_MOSI_PIN    11  // SPI MOSI
#define LCD_MISO_PIN    12  // SPI MISO (not used but must be defined)
#define LCD_RST_PIN     14  // Reset
#define LCD_BL_PIN      2   // Backlight
```

### Touch Pins (I2C)
```cpp
#define Touch_SDA_PIN   6   // I2C SDA
#define Touch_SCL_PIN   7   // I2C SCL
#define Touch_RST_PIN   13  // Reset
#define Touch_INT_PIN   5   // Interrupt
```

## Critical Configuration Changes

### 1. TFT_eSPI Library - DO NOT USE
**Problem**: TFT_eSPI causes NULL pointer crash on ESP32-S3
**Solution**: Use custom LCD driver (LCD_1in28.cpp/h from Waveshare)

### 2. LCD Driver Files (Required)
Copy these files from working example to project:
```
LCD_1in28.cpp
LCD_1in28.h
DEV_Config.cpp
DEV_Config.h
GUI_Paint.cpp
GUI_Paint.h
Debug.h
fonts.h
font8.cpp, font12.cpp, font16.cpp, font20.cpp, font24.cpp
ImageData.cpp
ImageData.h
CST816S.cpp
CST816S.h
```

### 3. LCD_1in28.cpp Critical Fixes

#### Fix 1: SetWindows Y-coordinate Bug (Line 389)
```cpp
// WRONG (original):
LCD_1IN28_SendData_8Bit((Xend-1)>>8);  // Bug: should be Yend

// CORRECT:
LCD_1IN28_SendData_8Bit((Yend-1)>>8);  // Fixed
```

#### Fix 2: MADCTL Rotation (Line 337)
```cpp
if(Scan_dir == HORIZONTAL) {
    LCD_1IN28.HEIGHT = LCD_1IN28_HEIGHT;
    LCD_1IN28.WIDTH  = LCD_1IN28_WIDTH;
    MemoryAccessReg = 0x48;  // MX + BGR (mirror X)
    // Other options to try if UI is misaligned:
    // 0x08 = BGR only (no flip)
    // 0x88 = MY + BGR (mirror Y)
    // 0xC8 = MX+MY + BGR (mirror both - original)
}
```

### 4. LVGL Display Flush Function

**CRITICAL**: Must use full-screen BlackImage buffer layout

```cpp
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    // Copy LVGL buffer to BlackImage at correct position (240x240 layout)
    uint16_t *src = (uint16_t *)&color_p->full;

    for(uint32_t row = 0; row < h; row++) {
        uint32_t y = area->y1 + row;
        uint32_t dst_offset = area->x1 + y * 240;  // 240 = screen width
        uint16_t *src_row = &src[row * w];

        // Swap bytes (GC9A01 needs big-endian RGB565)
        for(uint32_t col = 0; col < w; col++) {
            uint16_t color = src_row[col];
            BlackImage[dst_offset + col] = (color >> 8) | (color << 8);
        }
    }

    // Use original working function (inclusive→exclusive: +1)
    LCD_1IN28_DisplayWindows(area->x1, area->y1, area->x2 + 1, area->y2 + 1, BlackImage);

    lv_disp_flush_ready( disp_drv );
}
```

**Key Points**:
- Must copy to **BlackImage** (240x240 full-screen buffer)
- Must **swap bytes** (RGB565 high/low byte swap)
- Must use **correct offset** calculation: `x + y * 240`
- Must **+1 for Xend/Yend** (LVGL inclusive → DisplayWindows exclusive)

### 5. Setup Function Order

```cpp
void setup() {
    Serial.begin(115200);

    // 1. Initialize PSRAM (CRITICAL - needed for BlackImage)
    if(psramInit()) {
        Serial.println("PSRAM initialized");
    }

    // 2. Allocate BlackImage in PSRAM
    UDOUBLE Imagesize = LCD_1IN28_HEIGHT * LCD_1IN28_WIDTH * 2; // 115200 bytes
    BlackImage = (UWORD *)ps_malloc(Imagesize);

    // 3. Initialize GPIO and LCD hardware
    DEV_Module_Init();
    LCD_1IN28_Init(HORIZONTAL);

    // 4. Initialize LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    // 5. Register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 6. Initialize touch
    touch.begin();

    // 7. Register touch driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // 8. Setup LVGL timer
    esp_timer_create(...);
    esp_timer_start_periodic(...);

    // 9. Load SquareLine Studio UI
    ui_init();
}
```

### 6. SquareLine Studio Configuration

**Display Settings**:
- Width: 240
- Height: 240
- Color depth: 16bit RGB565
- `LV_COLOR_16_SWAP`: 0 (we swap manually in flush function)

**Export Settings**:
- LVGL version: 8.3.x
- Template: Arduino
- UI files location: Same folder as .ino

### 7. Arduino IDE Board Settings

```
Board: "ESP32S3 Dev Module"
Upload Speed: 921600
USB Mode: "Hardware CDC and JTAG"
USB CDC On Boot: "Enabled"
USB Firmware MSC On Boot: "Disabled"
USB DFU On Boot: "Disabled"
Upload Mode: "UART0 / Hardware CDC"
CPU Frequency: "240MHz (WiFi)"
Flash Mode: "QIO 80MHz"
Flash Size: "8MB (64Mb)"
Partition Scheme: "8M with spiffs (3MB APP/1.5MB SPIFFS)"
Core Debug Level: "None"
PSRAM: "QSPI PSRAM"
Arduino Runs On: "Core 1"
Events Run On: "Core 1"
```

## Common Issues & Solutions

### Issue 1: LCD shows nothing (black screen)
**Cause**: Backlight not enabled
**Solution**: Add backlight control in setup():
```cpp
pinMode(LCD_BL_PIN, OUTPUT);
digitalWrite(LCD_BL_PIN, HIGH);
```

### Issue 2: Text/UI is garbled or striped
**Cause**: Byte order mismatch
**Solution**: Ensure byte swap in flush function (see section 4)

### Issue 3: Black horizontal lines splitting UI
**Cause**: Multiple SPI transfers or wrong buffer layout
**Solution**: Use BlackImage full-screen buffer + LCD_1IN28_DisplayWindows

### Issue 4: UI position is wrong/mirrored
**Cause**: Wrong MADCTL rotation value
**Solution**: Try different MemoryAccessReg values (see section 3, Fix 2)

### Issue 5: Crash on tft.init() with TFT_eSPI
**Cause**: TFT_eSPI incompatible with this ESP32-S3 board
**Solution**: Use custom LCD driver (DO NOT use TFT_eSPI)

### Issue 6: Touch causes crash
**Cause**: CST816S driver issue or I2C conflict
**Solution**: Ensure CST816S.cpp is correct version, check I2C pins

## Memory Usage

- **BlackImage**: 115,200 bytes (240x240x2) in PSRAM
- **LVGL buffer**: ~5,760 bytes (240x240/10) in SRAM
- **Program**: ~570KB flash
- **Global variables**: ~85KB SRAM

## Performance Notes

- SPI Frequency: 40MHz (safe for GC9A01)
- LVGL tick: 2ms
- Refresh rate: ~30-60 FPS (depends on UI complexity)

## Testing Checklist

- [ ] Color test (RED/GREEN/BLUE/BLACK) works
- [ ] LVGL basic text renders correctly
- [ ] SquareLine UI displays without tearing
- [ ] No black lines/splits
- [ ] UI centered correctly
- [ ] Touch responds (if enabled)
- [ ] No crashes in loop()

## File Structure

```
ESP32S3_Squareline_UI/
├── ESP32S3_Squareline_UI.ino    # Main sketch
├── LCD_1in28.cpp/.h              # LCD driver
├── DEV_Config.cpp/.h             # Hardware config
├── GUI_Paint.cpp/.h              # Graphics library
├── CST816S.cpp/.h                # Touch driver
├── fonts.h, font*.cpp            # Font files
├── ImageData.cpp/.h              # Image data
├── Debug.h                       # Debug macros
├── ui.c/.h                       # SquareLine UI (auto-generated)
├── ui_Screen1.c/.h               # UI screens
├── ui_events.c/.h                # UI events
└── ui_helpers.c/.h               # UI helpers
```

## Reference Links

- ESP32-S3 Datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- GC9A01 Datasheet: (Round LCD controller)
- LVGL Documentation: https://docs.lvgl.io/8.3/
- SquareLine Studio: https://squareline.io/

---

**Last Updated**: 2025-10-31
**Tested Configuration**: Working successfully with ESP32-S3 + GC9A01 1.28" + LVGL 8.3.10
