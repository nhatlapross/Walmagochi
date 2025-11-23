#!/usr/bin/env python3
"""
Convert PNG images to LVGL C format
"""
from PIL import Image
import sys
import os

def png_to_lvgl_c(png_path, output_name):
    """Convert PNG to LVGL C array format (RGB565 with alpha)"""
    img = Image.open(png_path).convert('RGBA')
    width, height = img.size

    c_code = f"""// Generated from {os.path.basename(png_path)}
// Size: {width}x{height}

#include "lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

// ARGB8565 format: Alpha + RGB565
const LV_ATTRIBUTE_MEM_ALIGN uint8_t {output_name}_data[] = {{
"""

    pixels = img.load()
    byte_count = 0

    for y in range(height):
        c_code += "    "
        for x in range(width):
            r, g, b, a = pixels[x, y]

            # Convert RGB888 to RGB565
            r5 = (r >> 3) & 0x1F
            g6 = (g >> 2) & 0x3F
            b5 = (b >> 3) & 0x1F
            rgb565 = (r5 << 11) | (g6 << 5) | b5

            # ARGB8565: Alpha byte + RGB565 (2 bytes)
            c_code += f"0x{a:02X},"  # Alpha
            c_code += f"0x{(rgb565 >> 8):02X},0x{(rgb565 & 0xFF):02X},"  # RGB565 high, low

            byte_count += 3

            if (byte_count % 12) == 0:
                c_code += "\n    "

        if y < height - 1:
            c_code += "\n"

    c_code += f"""
}};

const lv_img_dsc_t {output_name} = {{
    .header = {{
        .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
        .always_zero = 0,
        .reserved = 0,
        .w = {width},
        .h = {height}
    }},
    .data_size = {width * height * 3},
    .data = {output_name}_data,
}};
"""

    return c_code

if __name__ == "__main__":
    idle_dir = "/home/alvin/Esp32-s3/src/sui_watch/assets/idle"
    output_dir = "/home/alvin/Esp32-s3/src/sui_watch"

    # Convert each image
    images = [
        ("2.png", "pet_idle_frame1"),
        ("3.png", "pet_idle_frame2"),
        ("4.png", "pet_idle_frame3"),
    ]

    for png_file, c_name in images:
        png_path = os.path.join(idle_dir, png_file)
        c_code = png_to_lvgl_c(png_path, c_name)

        output_file = os.path.join(output_dir, f"{c_name}.c")
        with open(output_file, 'w') as f:
            f.write(c_code)

        print(f"✓ Generated {output_file}")

    # Generate header file
    header = """#ifndef PET_SPRITES_H
#define PET_SPRITES_H

#include "lvgl.h"

// Pet idle animation frames
extern const lv_img_dsc_t pet_idle_frame1;
extern const lv_img_dsc_t pet_idle_frame2;
extern const lv_img_dsc_t pet_idle_frame3;

#endif // PET_SPRITES_H
"""

    header_file = os.path.join(output_dir, "pet_sprites.h")
    with open(header_file, 'w') as f:
        f.write(header)

    print(f"✓ Generated {header_file}")
    print("\n✅ All files generated successfully!")
