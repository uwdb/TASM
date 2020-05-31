import TileConfiguration_pb2
import os
from collections import namedtuple
from typing import List
import itertools

class Rectangle(object):
    def __init__(self, x, y, width, height):
        self.x = x
        self.y = y
        self.width = width
        self.height = height

    def __repr__(self):
        return f"x: {self.x}, y: {self.y}, width: {self.width}, height: {self.height}"


class TileLayout(object):
    def __init__(self, tile_config: TileConfiguration_pb2.TileConfiguration):
        self.number_of_rows = tile_config.numberOfRows
        self.number_of_columns = tile_config.numberOfColumns
        self.widths_of_columns = tile_config.widthsOfColumns
        self.heights_of_rows = tile_config.heightsOfRows

    @classmethod
    def from_file(cls, file_path):
        with open(file_path, 'rb') as f:
            serialized = f.read()

        config = TileConfiguration_pb2.TileConfiguration()
        config.ParseFromString(serialized)
        return TileLayout(config)

    def number_of_tiles(self):
        return self.number_of_columns * self.number_of_rows

    def total_width(self):
        return sum(self.widths_of_columns)

    def total_height(self):
        return sum(self.heights_of_rows)

    def rectangle_for_tile(self, tile: int) -> Rectangle:
        col = int(tile % self.number_of_columns)
        row = int(tile / self.number_of_columns)
        left_x_for_tile = sum(self.widths_of_columns[:col])
        top_y_for_tile = sum(self.heights_of_rows[:row])
        return Rectangle(left_x_for_tile, top_y_for_tile, self.widths_of_columns[col], self.heights_of_rows[row])


Interval = namedtuple('Interval', ['start', 'end'])
IntervalToLayout = namedtuple('IntervalToLayout', ['interval', 'tilelayout'])


class TileLayoutsManager(object):
    def __init__(self, dir_path):
        self.dir_path = dir_path

        self.intervals_and_layouts: List[IntervalToLayout] = []
        for subdir, dirs, files in os.walk(dir_path):
            if subdir == dir_path:
                continue

            dir_components = os.path.basename(subdir).split('-')
            interval = Interval(int(dir_components[0]), int(dir_components[1]))
            assert len(files) == 1
            serialized_tile_config = os.path.join(subdir, files[0])
            layout = TileLayout.from_file(serialized_tile_config)
            self.intervals_and_layouts.append(IntervalToLayout(interval, layout))

    def layout_for_frame(self, frame_num) -> TileLayout:
        def frame_in_interval(interval_to_layout):
            interval: Interval = interval_to_layout.interval
            return interval.start <= frame_num <= interval.end

        filtered: List[IntervalToLayout] = list(filter(frame_in_interval, self.intervals_and_layouts))
        assert len(filtered) == 1
        return filtered[0].tilelayout


if __name__ == '__main__':
    rootdir = '/Users/maureendaum/Downloads/traffic-1k-002-cracked-smalltiles-duration60'
    mngr = TileLayoutsManager(rootdir)
    mngr.layout_for_frame(0)
    print('here')

