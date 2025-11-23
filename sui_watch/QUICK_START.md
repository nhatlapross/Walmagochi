# Quick Start Guide - ESP32-S3 + GC9A01 LCD + LVGL

## 5-Minute Setup Checklist

### 1. Hardware Connections
```
LCD:  DC=8, CS=9, CLK=10, MOSI=11, MISO=12, RST=14, BL=2
Touch: SDA=6, SCL=7, RST=13, INT=5
```

### 2. Required Files (17 files total)
```bash
# LCD Driver (7 files)
LCD_1in28.cpp/.h, DEV_Config.cpp/.h, GUI_Paint.cpp/.h, Debug.h

# Fonts (6 files)
fonts.h, font8/12/16/20/24.cpp

# Touch (2 files)
CST816S.cpp/.h

# Image (2 files)
ImageData.cpp/.h
```

### 3. Critical Code Fixes

**LCD_1in28.cpp Line 389** - Fix Y coordinate bug:
```cpp
LCD_1IN28_SendData_8Bit((Yend-1)>>8);  // Was: (Xend-1)>>8
```

**LCD_1in28.cpp Line 341** - Set rotation:
```cpp
MemoryAccessReg = 0x48;  // MX + BGR
```

### 4. Display Flush Function
```cpp
void my_disp_flush(...) {
    // 1. Copy to BlackImage with byte swap
    for(row) {
        dst_offset = area->x1 + (area->y1 + row) * 240;
        for(col) {
            BlackImage[dst_offset + col] = (src[col] >> 8) | (src[col] << 8);
        }
    }

    // 2. Use original function (+1 for exclusive end)
    LCD_1IN28_DisplayWindows(x1, y1, x2+1, y2+1, BlackImage);
}
```

### 5. Arduino Board Settings
- **Board**: ESP32S3 Dev Module
- **PSRAM**: QSPI PSRAM (REQUIRED!)
- **Flash**: 8MB
- **Upload Speed**: 921600

### 6. SquareLine Studio Export
- **Size**: 240x240
- **Color**: RGB565 (16bit)
- **LV_COLOR_16_SWAP**: 0

## Common Mistakes to Avoid

❌ Using TFT_eSPI → ✅ Use custom LCD driver
❌ Forget byte swap → ✅ Swap in flush: `(color >> 8) | (color << 8)`
❌ Wrong buffer layout → ✅ Use BlackImage 240x240 full-screen
❌ Missing +1 for Xend/Yend → ✅ Always +1: `x2+1, y2+1`
❌ Forget PSRAM → ✅ Enable in board settings

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Black screen | Enable backlight: `digitalWrite(2, HIGH)` |
| Garbled text | Check byte swap in flush function |
| Black lines | Use BlackImage + LCD_1IN28_DisplayWindows |
| Wrong position | Try MADCTL: 0x08/0x48/0x88/0xC8 |
| Crash on init | Don't use TFT_eSPI, use LCD driver |

## Test Sequence

1. ✅ Upload → See color test (RED/GREEN/BLUE)
2. ✅ UI appears centered
3. ✅ No black lines/tearing
4. ✅ Touch responds (if enabled)

---
**Working Configuration Confirmed**: ESP32-S3 + GC9A01 + LVGL 8.3.10 ✓
