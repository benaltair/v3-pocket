"""Render firmware draw commands using the 5 by 7 pixel font used by the OLED."""
import re
import subprocess
from PIL import Image, ImageDraw


def render(program, library, output):
    font_file = next(library.rglob("glcdfont.c"))
    initializer = font_file.read_text().split("PROGMEM = {", 1)[1].split("};", 1)[0]
    initializer = re.sub(r"//[^\n]*|/\*.*?\*/", "", initializer, flags=re.S)
    font = bytes(int(x, 16) for x in re.findall(r"0x([a-fA-F0-9]{2})", initializer))
    if len(font) != 256 * 5:
        raise ValueError("Unexpected OLED font size")
    for args, groups, filename in [(["--traffic-preview"], ["quiet", "busy"], "traffic.png"),
                                   ([], ["home", "ready", "queued", "listening", "results", "cooldown"], "screens.png")]:
        pages = {}
        commands = subprocess.check_output([program, *args]).decode("latin1")
        for line in commands.splitlines():
            if line.startswith("PAGE "):
                image = Image.new("RGB", (128, 64), "black")
                pages[line[5:]] = image
                draw = ImageDraw.Draw(image)
            elif line.startswith("R "):
                x, y, w, h = map(int, line.split()[1:])
                draw.rectangle((x, y, x+w-1, y+h-1), fill="white")
            else:
                _, x, y, size, text = line.split(" ", 4)
                x, y, size = int(x), int(y), int(size)
                for char in text:
                    for i in range(5):
                        for j in range(8):
                            if font[ord(char)*5+i] & (1 << j):
                                a, b = x+i*size, y+j*size
                                draw.rectangle((a, b, a+size-1, b+size-1), fill="white")
                    x += 6 * size
        sheet = Image.new("RGB", (260, (len(groups)//2)*68-4), (42, 42, 42))
        for i, key in enumerate(groups):
            sheet.paste(pages[key], ((i%2)*132, (i//2)*68))
        sheet.resize((sheet.width*3, sheet.height*3), Image.Resampling.NEAREST).save(output / filename)
