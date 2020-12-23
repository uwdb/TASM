import os
import matplotlib.pyplot as plt
import numpy as np
import shlex
import subprocess

ROOT_PATH = "/home/maureen/black_tiles/"
MIN_WIDTH = 256
MAX_WIDTH = 3840
MIN_HEIGHT = 160
MAX_HEIGHT = 1980
ALIGNMENT = 32
FRAMERATE = 30

def aligned(dim):
    if not dim % ALIGNMENT:
        return dim
    return (int(dim / ALIGNMENT) + 1) * ALIGNMENT

def get_dir_path(width, height):
    return os.path.join(ROOT_PATH, f'{FRAMERATE}_f', f'{width}_w', f'{height}_h')

def get_frame_path(width, height):
    base_path = get_dir_path(width, height)
    frame_name = f'f_{width}_{height}.png'
    return os.path.join(base_path, frame_name)

def get_tile_path(width, height):
    base_path = get_dir_path(width, height)
    tile_name = f't_{width}_{height}.hevc'
    return os.path.join(base_path, tile_name)

def setup_dir(path):
    os.makedirs(path, exist_ok=True)

def save_frame(width, height, dir_path):
    frame = np.zeros((height, width, 3))
    plt.imsave(get_frame_path(width, height), frame)

def create_gop(width, height):
    tile_path = get_tile_path(width, height)
    frame_path = get_frame_path(width, height)
    print(f'Saving tile to {tile_path}')
    subprocess.run(shlex.split(
        f'ffmpeg -loop 1 '
        f'-i {frame_path} '
        f'-r {FRAMERATE} '
        f'-t 1 '
        f'-vcodec hevc_nvenc '
        f'{tile_path}'
    ))


def main():
    for tile_width in range(MIN_WIDTH, aligned(MAX_WIDTH)+1, ALIGNMENT):
        for tile_height in range(MIN_HEIGHT, aligned(MAX_HEIGHT)+1, ALIGNMENT):
            # Set up output directory.
            dir_path = get_dir_path(tile_width, tile_height)
            print(f'Setting up output path at {dir_path}')
            setup_dir(dir_path)

            # For each tile, first create an all-black frame.
            print(f'Saving frame')
            save_frame(tile_width, tile_height, dir_path)

            # Use the saved frame to create a single GOP.
            create_gop(tile_width, tile_height)


if __name__ == '__main__':
    main()
