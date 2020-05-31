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
from datetime import timedelta

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

def filter_command(num_rows, num_cols, coded_width, coded_height, display_height,
        use_uniform_tiles = False, row_heights = [], col_widths = []):
    cmd = ""
    total_height = 0
    used_row_heights = []
    PicHeightInCtbsY = math.ceil(coded_height / CTBS_SIZE_Y)
    uniform_tile_width = int(coded_width / num_cols)

    for i in range(num_rows * num_cols):
        tile_row = int(math.floor(i / num_cols))
        tile_col = int(i % num_cols)

        tile_width = uniform_tile_width if use_uniform_tiles else col_widths[tile_col] * CTBS_SIZE_Y
        cumulative_tile_widths = tile_col * uniform_tile_width if use_uniform_tiles else sum(col_widths[:tile_col]) * CTBS_SIZE_Y
        
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

        cmd += f"[0:v]crop={tile_width}:{row_height}:{cumulative_tile_widths}:{sum(used_row_heights[:-1])}[tile{i}];"

    cmd = cmd[:-1]
    return cmd

def map_command(num_tiles, video_tile_dir, encoding_strategy, framerate, tile_bitrates, frames_arg):
    cmd = ""
    output_files = []
    for i in range(num_tiles):
        output_file = f"{video_tile_dir}/tile_{i}.hevc"
        output_files.append(shlex.quote(os.path.abspath(output_file)))
        cmd += (f" -map \"[tile{i}]\" "
                f"-c:v hevc_nvenc -g {framerate} -r {framerate} -frames {frames_arg} "
                f"{encoding_strategy_to_ffmpeg_args(encoding_strategy, tile_bitrates[i])} "
                f"{shlex.quote(output_file)}")
    return cmd, output_files

def compute_tile_bitrates(original_bitrate, num_rows, num_cols, height, width, row_heights=None, col_widths=None):
    # row_heights, col_widths in CTBS
    # height, width in pixels
    num_tiles = num_rows * num_cols
    uniform_per_tile_bitrate = math.ceil(original_bitrate / num_tiles)
    if not row_heights and not col_widths:
        # In uniform tiling case.
        return [uniform_per_tile_bitrate] * num_tiles

    # In non-uniform case, need to compute on a per-tile basis the fraction of the total area.
    total_area_in_ctbs = to_ctbs(height) * to_ctbs(width)
    tile_bitrates = []
    for i in range(num_tiles):
        row_index = int(i / num_cols)
        col_index = int(i % num_cols)
        tile_area_in_ctbs = row_heights[row_index] * col_widths[col_index]
        tile_percent_area = tile_area_in_ctbs / total_area_in_ctbs
        tile_bitrates.append(max(math.ceil(tile_percent_area * original_bitrate),
                                uniform_per_tile_bitrate))

    return tile_bitrates

def remove_tile_directory(path):
    if not os.path.exists(path):
        return

    for dir_entry in os.scandir(path):
        if dir_entry.is_dir():
            remove_tile_directory(dir_entry.path)
        else:
            os.remove(dir_entry.path)
    
    if os.path.isdir(path):
        os.removedirs(path)

def to_ctbs(v):
    return math.ceil(v / CTBS_SIZE_Y)

def get_widths_and_heights_from_tile_configuration(path):
    layout = TileLayout.TileLayout.from_file(path)
    return ([to_ctbs(w) for w in layout.widths_of_columns],
             [to_ctbs(h) for h in layout.heights_of_rows])

class VideoStats(object):
    def __init__(self):
        self.width = -1
        self.coded_width = -1
        self.height = -1
        self.coded_height = -1
        self.bitrate = -1
        self.framerate = -1
        self.num_frames = -1

def get_video_stats(video):
    ffprobe_cmd = f"ffprobe -hide_banner -print_format json -show_format -show_streams {video}"
    ffprobe_output = subprocess.check_output(shlex.split(ffprobe_cmd)).decode('utf-8')
    ffprobe_output = json.loads(ffprobe_output)

    video_stats = VideoStats()

    streams = 'streams'
    video_stats.width = ffprobe_output[streams][0]['width']
    video_stats.coded_width = ffprobe_output[streams][0]['coded_width']
    assert video_stats.coded_width == video_stats.width

    video_stats.height = ffprobe_output[streams][0]['height']
    video_stats.coded_height = ffprobe_output[streams][0]['coded_height']
    if video_stats.coded_height % 32:
        video_stats.coded_height = int(video_stats.coded_height / 32 + 1) * 32 

    video_stats.bitrate = math.ceil(int(ffprobe_output['format']['bit_rate']) / 1000)

    framerate_fraction = ffprobe_output[streams][0]['r_frame_rate']
    dash_index = framerate_fraction.find('/')
    framerate_numerator = int(framerate_fraction[:dash_index])
    framerate_denominator = int(framerate_fraction[dash_index+1:])
    assert framerate_denominator == 1, "Unexpected denominator: %d" % framerate_denominator
    video_stats.framerate = framerate_numerator

    video_stats.num_frames = int(ffprobe_output[streams][0]['nb_frames'])

    return video_stats


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('-i', '--input', required=True, help='Path to input video')
    # ap.add_argument('-r', '--rows', required=True, type=int, help='Number of rows')
    # ap.add_argument('-c', '--columns', required=True, type=int,    help='Number of columns')
    ap.add_argument('-e', '--encode', required=False, default=1, type=int, help='Encode tiles')
    ap.add_argument('-s', '--stitch', required=False, default=1, type=int, help='Stitch tiles')
    ap.add_argument('-f', '--format', required=True, help='Encoding format for tiles')
    ap.add_argument('-dt', '--delete_tiles', required=False, default=1, type=int, help="Delete tiles and hevc")
    ap.add_argument("-dv", "--delete_video", required=False, default=1, type=int, help="Delete stitched mp4 video")
    ap.add_argument("-on", "--output_name", required=False, default=None, help="Stitched video name")
    ap.add_argument("-q", "--compute_quality", required=False, default=1, type=int, help="Quality should be computed")

    # Add arguments for non-uniform tiling.
    # ap.add_argument('-u', '--uniform', required=False, default=0, type=int, help='Use uniform tiles')
    ap.add_argument('-p', '--path', required=True, help='Path to directory containing frame ranges and tile layout binaries')
    ap.add_argument("-o", '--output', required=False, default=None, help="File to output CSV results")

    ap.add_argument('-qs', '--quality_start', required=False, help="Start timestamp for evaluating quality")
    ap.add_argument('-qe', '--quality_end', required=False, help="End timestamp for evaluating quality")

    args = vars(ap.parse_args())

    video = os.path.abspath(args['input'])
    encoding_strategy = input_arg_to_encoding_strategy(args['format'])

    use_uniform_tiles = False
    path_to_frames_and_layouts = os.path.abspath(args['path'])

    video_name = os.path.basename(video[:video.find('.')]) if not args["output_name"]  else args["output_name"]
    video_tile_dir = os.path.basename(path_to_frames_and_layouts)

    video_stats =  get_video_stats(video)

    stitched_segment_paths = []
    pps_id = 1

    if args['encode']:
        # Clean up old tiles
        remove_tile_directory(video_tile_dir)

        # Create directory for tiles
        if not os.path.isdir(video_tile_dir):
            os.makedirs(video_tile_dir)

    sorted_dirs = list(os.scandir(path_to_frames_and_layouts))
    sorted_dirs.sort(key=lambda d: int(d.name[:d.name.find('-')]) if d.is_dir() else -1)
    for dir_entry in sorted_dirs:
        if not dir_entry.is_dir():
            continue

        # Create sub-directory for tiles.
        output_dir = os.path.join(video_tile_dir, dir_entry.name)
        
        # Parse first and last frames from directory name.
        frames_re = re.search(r'^(\d+)-(\d+)', dir_entry.name)
        first_frame = int(frames_re.group(1))
        last_frame = int(frames_re.group(2))

        # if first_frame > 480:
        #     break

        # if first_frame != 480:
        #     continue

        if first_frame >= video_stats.num_frames:
            break # Safe to break because we sorted the files by the first frame.

        # Get path to tile-layout object.
        tile_layout_files = glob.glob(f"{dir_entry.path}/*.bin")
        assert len(tile_layout_files) == 1
        path_to_tile_configuration = tile_layout_files[0]
        (column_widths, row_heights) = get_widths_and_heights_from_tile_configuration(path_to_tile_configuration)
        num_rows = len(row_heights)
        num_cols = len(column_widths)
        num_tiles = num_rows * num_cols

        tile_files = []
        if args['encode']:
            # Make new directory for sub-tiles tiles.
            os.makedirs(output_dir)

            # Compute per-layout-duration encoding parameters.
            tile_bitrates = compute_tile_bitrates(video_stats.bitrate, num_rows, num_cols, video_stats.coded_height, video_stats.coded_width, row_heights, column_widths)
            filter_str = filter_command(num_rows, num_cols, video_stats.coded_width, video_stats.coded_height, video_stats.height, use_uniform_tiles, row_heights, column_widths)

            # map_command has to only operate over particular frames.
            # -ss: specifies time to skip to
            # -frames: specifies number of frames to operate over
            ss_arg = "{:0>8}".format(str(timedelta(seconds=first_frame / video_stats.framerate)))
            frames_arg = last_frame - first_frame + 1
            map_str, tile_files = map_command(num_tiles, output_dir, encoding_strategy, video_stats.framerate, tile_bitrates, frames_arg)
            
            # Create command to encode tiles
            encode_cmd = f"ffmpeg -y -ss {ss_arg} -i {video} -filter_complex \"{filter_str}\" {map_str}"
            print(encode_cmd)
            subprocess.run(shlex.split(encode_cmd))
        else:
            tile_files = [ shlex.quote(os.path.abspath(f"{output_dir}/tile_{i}.hevc")) for i in range(num_tiles) ]

        # Stitch these tiles.
        if (args['stitch']):
            stitcher_path = "/home/maureen/LightDBC++/LightDB/cmake-build-debug-remote/stitcher"
            stitched_video = f"stitched_{video_name}_{first_frame}_{last_frame}"
            stitched_hevc_path = f"{os.path.abspath(video_tile_dir)}/{stitched_video}.hevc"
            stitched_segment_paths.append(stitched_hevc_path)

            if num_tiles == 1:
                with open(stitched_hevc_path, 'wb') as stitched:
                    with open('../sei.txt', 'rb') as sei_file:
                        stitched.write(sei_file.read())
                    with open(tile_files[0], 'rb') as tile:
                        stitched.write(tile.read())
            else:
                stitch_cmd = (
                    f"{stitcher_path} "
                    f"{num_tiles} {' '.join(tile_files)} {num_rows} {num_cols} "
                    f"{video_stats.coded_height} {video_stats.coded_width} "
                    f"{video_stats.height} {video_stats.width} "
                    f"{shlex.quote(stitched_hevc_path)} "
                    f"{1 if use_uniform_tiles else 0} " )
                if not use_uniform_tiles:
                    stitch_cmd += (
                        f"{pps_id} "
                        f"{' '.join(list(map(str, row_heights))[:-1])} " 
                        f"{' '.join(list(map(str, column_widths))[:-1])}" )
                print(stitch_cmd)
                subprocess.run(shlex.split(stitch_cmd))

                pps_id += 1
                if pps_id >= 64:
                    pps_id = 1

    if args['stitch']:
        # Now that all of the tiles are processed, concatenate the HEVC files.
        stitched_video = f"stitched_{video_name}_{encoding_strategy.name}" \
                if not args["output_name"] else args["output_name"]
        with open(f"{stitched_video}.hevc", 'wb') as stitched_file:
            for section_path in stitched_segment_paths:
                with open(section_path, 'rb') as in_file:
                    stitched_file.write(in_file.read())

        subprocess.run(shlex.split(f"ffmpeg -hide_banner -y -r {video_stats.framerate} -i {stitched_video}.hevc -c:v copy {stitched_video}.mp4"))

        # Capture output.
        # Capture bitrate of stitched video, y_psnr, avg_psnr.
        if args["compute_quality"]:
            start_str = ""
            if args['quality_start']:
                start_str = f" -ss {args['quality_start']} "
            end_str = ""
            if args['quality_end']:
                end_str = f" -to {args['quality_end']} "

            psnr_output = subprocess.check_output(shlex.split(
                f"ffmpeg -hide_banner "
                f"{start_str} {end_str} -i {stitched_video}.mp4 "
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
            print(f"{video_name},stitcher,{encoding_strategy.name},{video_tile_dir},{int(use_uniform_tiles)},{psnr_avg},{psnr_y},{stitched_bitrate},{stitched_video_size},{ssim_y},{ssim_all},{ssim_all_db}", file=output_file)
            if output_file:
                output_file.close()

        # # # Clean up.
        if args['delete_tiles']:
            os.remove(f'{stitched_video}.hevc')
            for segment in stitched_segment_paths:
                os.remove(segment)
            remove_tile_directory(video_tile_dir)
        if args['delete_video']:
            os.remove(f'{stitched_video}.mp4')

