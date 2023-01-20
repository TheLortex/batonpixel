import math
import png
from perlin_noise import PerlinNoise

noise1 = PerlinNoise(octaves=3)
noise2 = PerlinNoise(octaves=6)
noise3 = PerlinNoise(octaves=12)
noise4 = PerlinNoise(octaves=24)
noise5 = PerlinNoise(octaves=48)

height = 144
width = 4 * height

baseColor = (0.2, 0.8, 0.7)  # 100 - 0
midColor = (0.9, 0.3, 0.75)  # 240 - 140/200
topColor = (0.3, 0.2, 0.8)  # 300 - 1

NOISE_SCALE = 4 * 144


img = []
for x in range(width):
    row = ()

    luminance = 255 * (1.3 + 1 * noise2(x / (NOISE_SCALE)) +
                       0.3 * noise3(x / NOISE_SCALE) +
                       0.2 * noise4(x / (NOISE_SCALE))) / 2.6
    for y in range(height):
        y = height - y + 3 * noise5([x / NOISE_SCALE, y / height])
        y = max(0, min(height - 1, y))

        c = [0, 0, 0]

        if y / height < 140 / 200:
            ratio = math.cos(math.pi / 2 * (y / height) / (140 / 200))
            a = baseColor
            b = midColor
        else:
            ratio = math.sin(math.pi / 2 * ((height - y) / height) /
                             ((200 - 140) / 200))
            a = midColor
            b = topColor

        l = max(0, min(255, (math.cos((math.pi / 2) * y / height / 1.3)) * luminance + 10 * noise2([x / NOISE_SCALE, y / height])))
        for i in range(3):
            c[i] = int(l * (a[i] * ratio + b[i] * (1 - ratio)))

        row = row + tuple(c)
    img.append(row)

imgT = []
for y in range(height):
    row = ()
    for x in range(width):
        row = row + (img[x][3 * y], img[x][3 * y + 1], img[x][3 * y + 2])
    imgT.append(row)

with open('gradient.png', 'wb') as f:
    w = png.Writer(width, height, greyscale=False)
    w.write(f, imgT)