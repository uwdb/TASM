#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <cstring>

#include "StitchContext.h"
#include "Stitcher.h"

using std::string;
using std::vector;
using stitching::StitchContext;
using stitching::Stitcher;

/* Usage */
// ./stitcher
// <number of tiles> - 1
// <path to each tile> - 2…num_tiles - 1 + 2
// <number of rows> - num_tiles + 2 <number of columns> - num_tiles + 3
// <height of final coded video> - + 4 <width of final coded video> - + 5
// <display height of video> - + 6 <display width of video> - + 7
// <path to stitched video> - + 8
// <Uniform tiles: 1 or 0> - + 9
// If not uniform: PPS_id - + 10
// If not uniform: <heights of first n-1 rows, in CTBs> - + 11
// If not uniform: <widths of first n-1 columns, in CTBs> - + 11 + num_rows - 1

int main(int argc, char *argv[]) {
    if (!strcmp(argv[1], "save_active_parameter_sets_sei")) {
        auto sei = Stitcher::GetActiveParameterSetsSEI();

        std::ofstream ostrm(argv[2]);
        for (auto c : *sei) {
            ostrm << c;
        }
        ostrm.close();
        return 0;
    }

    int num_tiles = std::stoi(argv[1]);
    vector<vector<char>> tiles(num_tiles);
    for (int i = 0; i < num_tiles; i++) {
        std::ifstream istrm;
        istrm.open(argv[i + 2]);
        std::stringstream buffer;
        buffer << istrm.rdbuf();
        string tile = buffer.str();
        tiles[i] = vector<char>(tile.begin(), tile.end());
    }

    std::pair<unsigned int, unsigned int> tile_dimensions;
    std::pair<unsigned int, unsigned int> video_dimensions;
    std::pair<unsigned int, unsigned int> video_display_dimensions;
    int number_of_rows = std::stoi(argv[num_tiles + 2]);
    int number_of_columns = std::stoi(argv[num_tiles + 3]);
    tile_dimensions.first = number_of_rows;
    tile_dimensions.second = number_of_columns;
    video_dimensions.first = std::stoi(argv[num_tiles + 4]);
    video_dimensions.second = std::stoi(argv[num_tiles + 5]);
    video_display_dimensions.first = std::stoi(argv[num_tiles + 6]);
    video_display_dimensions.second = std::stoi(argv[num_tiles + 7]);

    // See if uniform tiles should be used.
    bool should_use_uniform_tiles = std::stoi(argv[num_tiles + 9]);
    unsigned int pps_id = 0;

    std::vector<unsigned int> tile_heights;
    std::vector<unsigned int> tile_widths;
    if (!should_use_uniform_tiles) {
        pps_id = std::stoi(argv[num_tiles + 10]);

        // Read in tile heights and tile widths.
        tile_heights.resize(number_of_rows - 1);
        for (int i = 0; i < number_of_rows - 1; ++i)
            tile_heights[i] = std::stoi(argv[num_tiles + 11 + i]);

        tile_widths.resize(number_of_columns - 1);
        for (int i = 0; i < number_of_columns - 1; ++i)
            tile_widths[i] = std::stoi(argv[num_tiles + 10 + number_of_rows - 1 + 1 + i]);
    }

    StitchContext context(tile_dimensions, video_dimensions, video_display_dimensions, should_use_uniform_tiles, tile_heights, tile_widths, pps_id);
    Stitcher stitcher(context, tiles);
    auto stitched = stitcher.GetStitchedSegments();

    std::ofstream ostrm;
    ostrm.open(argv[num_tiles + 8]);
    for (auto c : *stitched) {
        ostrm << c;
    }
    ostrm.close();

    /**
    std::cout << "correct" << std::endl;
    std::ifstream istrm;
    istrm.open(argv[num_tiles + 7]);
    std::stringstream buffer;
    buffer << istrm.rdbuf();
    string correct = buffer.str();
    for (auto c : correct) {
        std::cout << static_cast<int>(static_cast<unsigned char>(c)) << std::endl;
    } **/
    return 0;
     /**
    testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS(); **/
}