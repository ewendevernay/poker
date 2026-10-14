from PIL import Image, ImageDraw, ImageFont

card_ratio = 5/7

card_height = 500
card_width = int(card_height * card_ratio)

final_canvas = Image.new('RGB', (card_width * 14, card_height * 4), color='white')

all_suits_image = Image.open('suits.png')
suit_width = all_suits_image.size[0] / 2
suit_height = all_suits_image.size[1] / 2

suits_images = [
    all_suits_image.crop((int((i % 2) * suit_width), int((i // 2) * suit_height),
                          int(((i % 2) + 1) * suit_width), int(((i // 2) + 1) * suit_height)))
    for i in range(4)]

def center_around(pos, width, height=0):
    return (int(pos[0] - width / 2), int(pos[1] - height / 2))

number_position = (card_width // 10, -10)
font_size = card_width // 5

font = ImageFont.truetype("./garamond.ttf", font_size)

for number in range(13):
    for color in range(4):
        temp_size = (card_width, card_height)  # size of the temporary canvas
        canvas = Image.new('RGBA', temp_size, (0, 0, 0, 0))  # fully transparent
        draw = ImageDraw.Draw(canvas)

        if number < 10:
            number_text = str(1 + number)
        else:
            number_text = ["J", "Q", "K"][number - 10]

        number_width, number_height = draw.textsize(number_text, font=font)

        if color == 0 or color == 3:
            text_color = "black"
        else:
            text_color = "#d40000"

        draw.text(center_around(number_position, number_width),
                  number_text, fill=text_color, font=font)

        suit = suits_images[color].copy()
        suit_width = card_width // 10
        suit.thumbnail((suit_width, card_width * 10))

        suit_height = suit.size[1]

        canvas.paste(suit,
                     center_around((number_position[0],
                                    number_position[1] + number_height + suit_height // 3),
                                   suit_width),
                     suit)

        rotated_canvas = canvas.rotate(180, expand=True)
        canvas.paste(rotated_canvas, (0, 0), rotated_canvas)

        suit = suits_images[color].copy()

        big_suit_width = suit_width * 4
        big_suit_height = suit_height * 4
        suit.thumbnail((big_suit_width, big_suit_height))

        canvas.paste(suit,
                     center_around((card_width / 2, card_height / 2),
                                   big_suit_width, big_suit_height),
                     suit)

        final_canvas.paste(canvas, ((number + 1) * card_width, color * card_height), canvas)

#rotated_suit = suit.rotate(45, expand=True)
#canvas.paste(rotated_suit, (50, 350), rotated_suit)

final_canvas.save('output.png')
print("Image saved as output.png")
