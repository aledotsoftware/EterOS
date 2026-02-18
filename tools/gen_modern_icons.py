
import os
import math
from PIL import Image, ImageDraw, ImageFont

# Ensure directory exists
OUTPUT_DIR = "initrd_root"
if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

def draw_rounded_rect(draw, box, radius, fill):
    x, y, w, h = box
    draw.rounded_rectangle((x, y, x+w, y+h), radius=radius, fill=fill)

def get_gradient_fill(width, height, c1, c2):
    base = Image.new('RGB', (width, height), c1)
    top = Image.new('RGB', (width, height), c2)
    mask = Image.new('L', (width, height))
    mask_data = []
    for y in range(height):
        for x in range(width):
            mask_data.append(int(255 * (y / height)))
    mask.putdata(mask_data)
    base.paste(top, (0, 0), mask)
    return base

# 1. Launcher Icon (Grid)
def gen_launcher():
    size = (48, 48)
    # Background: Dark Glass
    img = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Rounded bg
    draw.rounded_rectangle((2, 2, 46, 46), radius=12, fill=(30, 41, 59, 220)) # Dark Blue-Grey
    
    # 3x3 Grid of white dots/squares
    dot_color = (255, 255, 255, 255)
    start_x, start_y = 13, 13
    gap = 10
    dot_size = 4
    for row in range(3):
        for col in range(3):
            x = start_x + col * gap
            y = start_y + row * gap
            draw.rectangle((x, y, x+dot_size, y+dot_size), fill=dot_color)
            
    img.save(os.path.join(OUTPUT_DIR, "icon_launcher.png"))
    print(f"Generated {OUTPUT_DIR}/icon_launcher.png")

# 2. GIMP Icon (Linux)
def gen_gimp():
    size = (48, 48)
    img = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Background: Grey/Brown for Wilbur feel
    draw.rounded_rectangle((2, 2, 46, 46), radius=12, fill=(80, 80, 80, 255))
    
    # Paintbrush bristles (colorful top)
    draw.rectangle((10, 10, 18, 20), fill=(255, 0, 0)) # Red
    draw.rectangle((20, 10, 28, 20), fill=(0, 255, 0)) # Green
    draw.rectangle((30, 10, 38, 20), fill=(0, 0, 255)) # Blue
    
    # Handle
    draw.rectangle((10, 20, 38, 38), fill=(200, 200, 200))
    
    img.save(os.path.join(OUTPUT_DIR, "icon_gimp.png"))
    print(f"Generated {OUTPUT_DIR}/icon_gimp.png")

# 3. Play Store (Android)
def gen_play():
    size = (48, 48)
    img = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Background: White
    draw.rounded_rectangle((2, 2, 46, 46), radius=12, fill=(255, 255, 255, 255))
    
    # Triangle
    # Points: (14, 10), (14, 38), (38, 24)
    # Colors: Blue (left), Green (top-right), Red (bottom-right), Yellow (accents) - simplified
    
    draw.polygon([(14, 10), (14, 38), (38, 24)], fill=(0, 200, 255)) # Cyan-ish
    
    img.save(os.path.join(OUTPUT_DIR, "icon_play.png"))
    print(f"Generated {OUTPUT_DIR}/icon_play.png")

# 4. Terminal
def gen_term():
    size = (48, 48)
    img = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Background: Black
    draw.rounded_rectangle((2, 2, 46, 46), radius=8, fill=(30, 30, 30, 255))
    draw.rectangle((2, 2, 46, 12), fill=(60, 60, 60, 255)) # Title bar
    
    # Prompt >_
    draw.text((10, 16), ">_", fill=(0, 255, 0, 255)) # Default font
    
    img.save(os.path.join(OUTPUT_DIR, "icon_term.png"))
    print(f"Generated {OUTPUT_DIR}/icon_term.png")

if __name__ == "__main__":
    gen_launcher()
    gen_gimp()
    gen_play()
    gen_term()
