from PIL import Image, ImageDraw, ImageFont

def center_around(pos, width, height=0):
    return (int(pos[0] - width / 2), int(pos[1] - height / 2))

def draw_diamond_polygon(draw, center_x, center_y, size, outline=None, fill=None, width=1):
    half_diag = size / 2

    points = [
        (center_x, center_y - half_diag),
        (center_x + half_diag, center_y),
        (center_x, center_y + half_diag),
        (center_x - half_diag, center_y),
    ]
    draw.polygon(points, outline=outline, fill=fill, width=width)

# Max: 3072 x 1920
    
image_width = 3072 + 100#1600
image_height = 1920 + 100#900
    
canvas = Image.new('RGB', (image_width, image_height), color='white')
draw = ImageDraw.Draw(canvas)

all_suits_image = Image.open('suits.png')
suit_width = all_suits_image.size[0] / 2
suit_height = all_suits_image.size[1] / 2

suits_images = [
    all_suits_image.crop((int((i % 2) * suit_width), int((i // 2) * suit_height),
                          int(((i % 2) + 1) * suit_width), int(((i // 2) + 1) * suit_height)))
    for i in range(4)]

suit_width = 35

for i in range(len(suits_images)):
    suits_images[i].thumbnail((suit_width, 900))

diamond_size = 100

y_len = (image_height // diamond_size) * 2 + 1
x_len = image_width // diamond_size + 1

palette = ["#178d5d", "#0b6750"]

for y in range(y_len):
    for x in range(x_len):
        if y % 2 == 0:
            start = diamond_size // 2
        else:
            start = 0

        pos_x = start + x * diamond_size
        pos_y = y * diamond_size // 2
            
        draw_diamond_polygon(draw,
                             pos_x,
                             pos_y,
                             diamond_size,
                             fill=palette[y % 2])
        
        suit = suits_images[(x * y_len + y) % 4]
        colored = Image.new('RGBA', suit.size, palette[(y + 1) % 2])
        colored.putalpha(suit.split()[3])
        
        canvas.paste(colored, center_around((pos_x, pos_y), suit.size[0], suit.size[1]), suit)

canvas.save('output.png')

# Color palette:
# Light green: #178d5d
# Dark green: #0b6750
