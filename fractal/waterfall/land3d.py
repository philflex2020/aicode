import pygame
import math

WIDTH, HEIGHT = 800, 600
pygame.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("3D-Like Terrain")

# Parameters
GRID_W, GRID_H = 50, 50   # grid size
SCALE = 20                # horizontal spacing
HEIGHT_SCALE = 50         # how tall hills are
SPEED = 0.05              # camera move speed

# Simple height function
def height_function(x, y, t):
    return math.sin(0.2 * x + t) * math.cos(0.2 * y + t) * HEIGHT_SCALE

def project(x, y, z):
    """Simple perspective projection"""
    fov = 300  # focal length
    factor = fov / (fov + z)
    screen_x = WIDTH // 2 + int(x * factor)
    screen_y = HEIGHT // 2 - int(y * factor)
    return screen_x, screen_y

def draw_terrain(time_offset):
    screen.fill((0, 0, 0))
    points = []

    # Generate grid
    for gx in range(-GRID_W//2, GRID_W//2):
        row = []
        for gy in range(-GRID_H//2, GRID_H//2):
            # world coordinates
            wx = gx * SCALE
            wy = gy * SCALE
            wz = height_function(gx, gy, time_offset)

            # add depth offset so terrain moves
            wz += time_offset * 50

            # project to screen
            sx, sy = project(wx, wy, wz + 200)  # shift z to avoid too close
            row.append((sx, sy))
        points.append(row)

    # Draw wireframe
    color = (0, 200, 255)
    for x in range(len(points) - 1):
        for y in range(len(points[x]) - 1):
            pygame.draw.line(screen, color, points[x][y], points[x+1][y])
            pygame.draw.line(screen, color, points[x][y], points[x][y+1])

clock = pygame.time.Clock()
running = True
t = 0.0

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    draw_terrain(t)
    pygame.display.flip()
    t += SPEED
    clock.tick(30)

pygame.quit()
