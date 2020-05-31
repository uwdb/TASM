#!/usr/bin/env python3

import argparse
import subprocess
import shlex
import json
import pprint
import math
import os
import glob
from enum import Enum, auto
import re

import TileLayout
import TileConfiguration_pb2

CTBS_SIZE_Y = 32

class EncodingStrategy(Enum):
    CONSTQP = auto()
    CBR = auto()
    VBR = auto()
    VBV = auto()
    NONE = auto()
    DEFAULT_CONSTQP = auto()
    VBR_CQ23 = auto()
    VBR_LOOKAHEAD = auto()

def input_arg_to_encoding_strategy(input):
    if input == 'constqp':
        return EncodingStrategy.CONSTQP
    elif input == 'cbr':
        return EncodingStrategy.CBR
    elif input == 'vbr':
        return EncodingStrategy.VBR
    elif input == 'vbv':
        return EncodingStrategy.VBV
    elif input == 'none':
        return EncodingStrategy.NONE
    elif input == 'default_constqp':
        return EncodingStrategy.DEFAULT_CONSTQP
    elif input == 'vbr_cq23':
        return EncodingStrategy.VBR_CQ23
    elif input == 'vbr_lookahead':
        return EncodingStrategy.VBR_LOOKAHEAD
    else:
        assert False, "Unrecognized encoding strategy: %r" % input

def encoding_strategy_to_ffmpeg_args(encoding_strategy, bitrate=-1):
    if encoding_strategy is EncodingStrategy.CONSTQP:
        return "-rc constqp -qp 28"
    elif encoding_strategy is EncodingStrategy.DEFAULT_CONSTQP:
        return "-rc constqp -qp -1"
    elif encoding_strategy is EncodingStrategy.CBR:
        assert bitrate > 0
        return f"-rc cbr -cbr 1 -b:v {bitrate}k -maxrate {bitrate}k -minrate {bitrate}k -bufsize {2 * bitrate}k"
    elif encoding_strategy is EncodingStrategy.VBR: # Maybe try specifying bufsize?
        assert bitrate > 0
        return f"-rc vbr_hq -b:v {bitrate}K -cq 28"
    elif encoding_strategy is EncodingStrategy.VBV:
        assert bitrate > 0
        return f"-rc vbr -preset hq -b:v {bitrate}K -maxrate {2*bitrate}K -bufsize {4*bitrate}K"
    elif encoding_strategy is EncodingStrategy.NONE:
        return ""
    elif encoding_strategy is EncodingStrategy.VBR_CQ23:
        assert bitrate > 0
        return f"-rc vbr_hq -b:v {bitrate}K -bufsize {4*bitrate}K  -cq 23" # -bufsize {4*bitrate}K
    elif encoding_strategy is EncodingStrategy.VBR_LOOKAHEAD:
        assert bitrate > 0
        return f"-rc vbr -b:v {bitrate}K -bufsize {4*bitrate}K -rc-lookahead 32 -no-scenecut 1"
    else:
        assert False, "Unsupported encoding strategy %r" % encoding_strategy

def filter_command(num_rows, num_cols, coded_width, coded_height, display_height, display_width,
        use_uniform_tiles = False, row_heights = [], col_widths = []):
    cmd = ""
    total_height = 0
    used_row_heights = []
    total_width = 0
    used_col_widths = []
    PicHeightInCtbsY = math.ceil(coded_height / CTBS_SIZE_Y)
    PicWidthInCtbsY = math.ceil(coded_width / CTBS_SIZE_Y)
    uniform_tile_width = int(coded_width / num_cols)

    for i in range(num_rows * num_cols):
        tile_row = int(math.floor(i / num_cols))
        tile_col = int(i % num_cols)

        # tile_width = uniform_tile_width if use_uniform_tiles else col_widths[tile_col] * CTBS_SIZE_Y
        # cumulative_tile_widths = tile_col * uniform_tile_width if use_uniform_tiles else sum(col_widths[:tile_col]) * CTBS_SIZE_Y
        row_height = -1
        col_width = -1
        if len(used_row_heights) <= tile_row:
            row_height = math.floor(((tile_row + 1) * PicHeightInCtbsY) / num_rows) \
                        - math.floor(tile_row * PicHeightInCtbsY / num_rows) \
                        if use_uniform_tiles \
                        else row_heights[tile_row]
            row_height *= CTBS_SIZE_Y
            if display_height - sum(used_row_heights) - row_height < 0:
                row_height = display_height - sum(used_row_heights)
            used_row_heights.append(row_height)
        else:
            row_height = used_row_heights[tile_row]

        if len(used_col_widths) <= tile_col:
            col_width = math.floor(((tile_col + 1) * PicWidthInCtbsY) / num_cols) \
                            - math.floor(tile_col * PicWidthInCtbsY / num_cols) \
                        if use_uniform_tiles \
                            else col_widths[tile_col]
            col_width *= CTBS_SIZE_Y
            if display_width - sum(used_col_widths) - col_width < 0:
                col_width = display_width - sum(used_col_widths)
            used_col_widths.append(col_width)
        else:
            col_width = used_col_widths[tile_col]

        cmd += f"[0:v]crop={col_width}:{row_height}:{sum(used_col_widths[:tile_col])}:{sum(used_row_heights[:-1])}[tile{i}];"

    cmd = cmd[:-1]
    return cmd

def map_command(num_tiles, video_tile_dir, encoding_strategy, framerate, tile_bitrates):
    cmd = ""
    output_files = []
    for i in range(num_tiles):
        output_file = f"{video_tile_dir}/tile_{i}.hevc"
        output_files.append(os.path.abspath(output_file))
        cmd += (f" -map \"[tile{i}]\" "
                f"-c:v hevc_nvenc -g {framerate} -r {framerate} "
                f"{encoding_strategy_to_ffmpeg_args(encoding_strategy, tile_bitrates[i])} "
                f"{output_file}")
    return cmd, output_files

def compute_tile_bitrates(original_bitrate, num_rows, num_cols, height, width, row_heights=None, col_widths=None):
    # row_heights, col_widths in CTBS
    # height, width in pixels
    num_tiles = num_rows * num_cols
    if not row_heights and not col_widths:
        # In uniform tiling case.
        uniform_per_tile_bitrate = math.ceil(original_bitrate / num_tiles)
        return [uniform_per_tile_bitrate] * num_tiles

    # In non-uniform case, need to compute on a per-tile basis the fraction of the total area.
    total_area_in_ctbs = to_ctbs(height) * to_ctbs(width)
    tile_bitrates = []
    for i in range(num_tiles):
        row_index = int(i / num_cols)
        col_index = int(i % num_cols)
        tile_area_in_ctbs = row_heights[row_index] * col_widths[col_index]
        tile_percent_area = tile_area_in_ctbs / total_area_in_ctbs
        tile_bitrates.append(math.ceil(tile_percent_area * original_bitrate))

    return tile_bitrates

def remove_tile_directory(path):
    for file in glob.glob(f"{path}/*.hevc"):
        os.remove(file)
    for file in glob.glob(f"{path}/*.mp4"):
        os.remove(file)
    
    if os.path.isdir(path):
        os.removedirs(path)

def to_ctbs(v):
    return math.ceil(v / CTBS_SIZE_Y)

def get_widths_and_heights_from_tile_configuration(path):
    layout = TileLayout.TileLayout.from_file(path)
    return ([to_ctbs(w) for w in layout.widths_of_columns],
             [to_ctbs(h) for h in layout.heights_of_rows])


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('-i', '--input', required=True, help='Path to input video')
    ap.add_argument('-r', '--rows', required=True, type=int, help='Number of rows')
    ap.add_argument('-c', '--columns', required=True, type=int,    help='Number of columns')
    ap.add_argument('-e', '--encode', required=False, default=1, type=int, help='Encode tiles')
    ap.add_argument('-s', '--stitch', required=False, default=1, type=int, help='Stitch tiles')
    ap.add_argument('-f', '--format', required=True, help='Encoding format for tiles')
    ap.add_argument('-dt', '--delete_tiles', required=False, default=1, type=int, help="Delete tiles")
    ap.add_argument('-dv', '--delete_video', required=False, default=1, type=int, help="Delete stitched video")
    ap.add_argument('-on', '--output_name', required=False, default=None, help="Name of stitched video")
    ap.add_argument('-q', '--quality', required=False, default=1, type=int, help="Should compute quality")

    # Add arguments for non-uniform tiling.
    ap.add_argument('-u', '--uniform', required=False, default=1, type=int, help='Use uniform tiles')
    ap.add_argument('-m', '--row_heights', required=False, type=int, nargs='+')
    ap.add_argument('-w', '--column_widths', required=False, type=int, nargs='+')
    ap.add_argument('-p', '--path', required=False, default='', help='Path to TileLayout binary')

    ap.add_argument("-o", '--output', required=False, default=None, help="File to output CSV results")

    ap.add_argument('-qs', '--quality_start', required=False, help="Start timestamp for evaluating quality")
    ap.add_argument('-qe', '--quality_end', required=False, help="End timestamp for evaluating quality")

    args = vars(ap.parse_args())

    video = os.path.abspath(args['input'])
    num_rows = args['rows']
    num_cols = args['columns']
    num_tiles = num_rows * num_cols
    encoding_strategy = input_arg_to_encoding_strategy(args['format'])

    use_uniform_tiles = bool(args['uniform'])
    row_heights = args['row_heights']
    column_widths = args['column_widths']
    path_to_tile_layout = args['path']
    should_get_layout_from_path = len(path_to_tile_layout)
    if not use_uniform_tiles and not should_get_layout_from_path:
        assert (use_uniform_tiles and not len(row_heights)) or (not use_uniform_tiles and len(row_heights) == num_rows)
        assert (use_uniform_tiles and not len(column_widths)) or (not use_uniform_tiles and len(column_widths) == num_cols)
    elif not use_uniform_tiles:
        (column_widths, row_heights) = get_widths_and_heights_from_tile_configuration(path_to_tile_layout)

    video_name = os.path.basename(video[:video.find('.')]) if not args["output_name"]  else args["output_name"]
    video_tile_dir = f"{video_name}_{num_rows}x{num_cols}"

    ffprobe_cmd = f"ffprobe -hide_banner -print_format json -show_format -show_streams {video}"
    ffprobe_output = subprocess.check_output(shlex.split(ffprobe_cmd)).decode('utf-8')
    ffprobe_output = json.loads(ffprobe_output)

    streams = 'streams'
    width = ffprobe_output[streams][0]['width']
    coded_width = ffprobe_output[streams][0]['coded_width']
    assert coded_width == width

    height = ffprobe_output[streams][0]['height']
    coded_height = ffprobe_output[streams][0]['coded_height']
    if coded_height % 32:
        coded_height = int(coded_height / 32 + 1) * 32

    bitrate = math.ceil(int(ffprobe_output['format']['bit_rate']) / 1000)
    tile_bitrates = compute_tile_bitrates(bitrate, num_rows, num_cols, coded_height, coded_width, row_heights, column_widths)

    framerate_fraction = ffprobe_output['streams'][0]['r_frame_rate']
    dash_index = framerate_fraction.find('/')
    framerate_numerator = int(framerate_fraction[:dash_index])
    framerate_denominator = int(framerate_fraction[dash_index+1:])
    assert framerate_denominator == 1, "Unexpected denominator: %d" % framerate_denominator
    framerate = framerate_numerator

    filter_str = filter_command(num_rows, num_cols, coded_width, coded_height, height, width, use_uniform_tiles, row_heights, column_widths)
    map_str, tile_files = map_command(num_tiles, video_tile_dir, encoding_strategy, framerate, tile_bitrates)

    if (args['encode']):
        # Clean up old tiles
        remove_tile_directory(video_tile_dir)

        # Create directory for tiles
        if not os.path.isdir(video_tile_dir):
            os.makedirs(video_tile_dir)
        
        # Create command to encode tiles
        encode_cmd = f"ffmpeg -y -i {video} -filter_complex \"{filter_str}\" {map_str}"
        print(encode_cmd)
        subprocess.run(shlex.split(encode_cmd))

    if (args['stitch']):
        stitcher_path = "/home/maureen/LightDBC++/LightDB/cmake-build-debug-remote/stitcher"
        stitched_video = f"stitched_{video_name}_{encoding_strategy.name}_{num_rows}x{num_cols}" if args["output_name"] is None else args["output_name"]

        if os.path.exists(f"{stitched_video}.mp4"):
            os.remove(f"{stitched_video}.mp4")

        stitch_cmd = (
            f"{stitcher_path} "
            f"{num_tiles} {' '.join(tile_files)} {num_rows} {num_cols} "
            f"{coded_height} {coded_width} "
            f"{height} {width} "
            f"{os.path.abspath(os.path.curdir)}/{stitched_video}.hevc "
            f"{1 if use_uniform_tiles else 0} " )
        if not use_uniform_tiles:
            stitch_cmd += (
                f"{' '.join(list(map(str, row_heights))[:-1])} " 
                f"{' '.join(list(map(str, column_widths))[:-1])}" )

        if num_tiles > 1:
            print(stitch_cmd)
            subprocess.run(shlex.split(stitch_cmd))
        else:
            assert len(tile_files) == 1
            subprocess.run(shlex.split(f'cp {tile_files[0]} {stitched_video}.hevc'))
        subprocess.run(shlex.split(f"ffmpeg -hide_banner -y -r {framerate} -i {stitched_video}.hevc -c:v copy {stitched_video}.mp4"))

        if args['quality']:
            start_str = ""
            if args['quality_start']:
                start_str = f" -ss {args['quality_start']} "
            end_str = ""
            if args['quality_end']:
                end_str = f" -to {args['quality_end']} "

            # Capture output.
            # Capture bitrate of stitched video, y_psnr, avg_psnr.
            psnr_output = subprocess.check_output(shlex.split(
                f"ffmpeg -hide_banner {start_str} {end_str} -i {stitched_video}.mp4 "
                f"{start_str} {end_str} -i {video} "
                                "-lavfi \"ssim;[0:v][1:v]psnr\" -f null -"), stderr=subprocess.STDOUT).decode('utf-8')
            bitrate_match = re.search(r'Stream #0:0.*, (\d+) kb/s', psnr_output)
            stitched_bitrate = int(bitrate_match.group(1))

            psnr_matches = re.search(r'.*PSNR y:([\d, \.]+).*average:([\d, \.]+)', psnr_output)
            psnr_y = float(psnr_matches.group(1))
            psnr_avg = float(psnr_matches.group(2))

            ssim_matches = re.search(r'.*SSIM Y:([\d, \.]+).*All:([\d, \.]+) \(([\d, .]+)\)', psnr_output)
            ssim_y = float(ssim_matches.group(1))
            ssim_all = float(ssim_matches.group(2))
            ssim_all_db = float(ssim_matches.group(3))

            stitched_video_size = os.path.getsize(f"{stitched_video}.mp4")

            # Print output, to CSV if output file is provided.
            output_file = None
            if args['output'] is not None:
                output_file = open(args['output'], 'a')
            print(f"{video_name},stitcher,{encoding_strategy.name},{num_rows},{num_cols},{int(use_uniform_tiles)},{psnr_avg},{psnr_y},{stitched_bitrate},{stitched_video_size},{ssim_y},{ssim_all},{ssim_all_db}", file=output_file)
            if output_file:
                output_file.close()

        # # # Clean up.
        if args['delete_tiles'] == 1:
            remove_tile_directory(video_tile_dir)
            os.remove(f'{stitched_video}.hevc')
        if args['delete_video'] == 1:
            os.remove(f'{stitched_video}.mp4')
