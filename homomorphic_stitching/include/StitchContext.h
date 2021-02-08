#ifndef HOMOMORPHIC_STITCHING_STITCHCONTEXT_H
#define HOMOMORPHIC_STITCHING_STITCHCONTEXT_H

#include <utility>
#include <vector>

namespace stitching {

    class StitchContext {
    public:

        /**
         * Constructs a context with the given tile and video dimensions
         * @param tile_dimensions The tile dimensions, first element being the height (number of rows)
         * and the second being the width (number of columns)
         * @param video_dimensions  The video dimensions, first element being the height and the second
         * the width
         */
        StitchContext(const std::pair<unsigned int, unsigned int>& tile_dimensions,
                      const std::pair<unsigned int, unsigned int>& coded_video_dimensions,
                      const std::pair<unsigned int, unsigned int> &display_video_dimensions={},
                      bool should_use_uniform_tiles=true,
                      const std::vector<unsigned int> &heights_of_tiles={},
                      const std::vector<unsigned int> &widths_of_tiles={},
                      unsigned int pps_id=0)
                : tile_dimensions_{tile_dimensions},
                  coded_video_dimensions_{coded_video_dimensions},
                  display_video_dimensions_(display_video_dimensions),
                  should_use_uniform_tiles_(should_use_uniform_tiles),
                  heights_of_tiles_(heights_of_tiles),
                  widths_of_tiles_(widths_of_tiles),
                  pps_id_(pps_id)
        { }

        /**
         *
         * @return The tile dimensions. Note that this gives the user the ability to change the dimensions
         */
        // TODO change to using dimensions=std::pair
        inline const std::pair<unsigned int, unsigned int> GetTileDimensions() const {
            return tile_dimensions_;
        }
        /**
         *
         * @return The video dimensions. Note that this gives the user the ability to change the dimensions
         */
        inline const std::pair<unsigned int, unsigned int>& GetVideoDimensions() const {
            return coded_video_dimensions_;
        }

        unsigned int GetVideoCodedWidth() const { return coded_video_dimensions_.second; }
        unsigned int GetVideoCodedHeight() const { return coded_video_dimensions_.first; }


        unsigned int GetVideoDisplayWidth() const { return display_video_dimensions_.second; };
        unsigned int GetVideoDisplayHeight() const { return display_video_dimensions_.first; };

        const std::vector<unsigned int > &GetHeightsOfTiles() const { return heights_of_tiles_; }
        const std::vector<unsigned int> &GetWidthsOfTiles() const { return widths_of_tiles_; }
        bool GetShouldUseUniformTiles() const { return should_use_uniform_tiles_; }

        unsigned int GetPPSId() const { return pps_id_; }

    private:

        const std::pair<unsigned int, unsigned int> tile_dimensions_;
        const std::pair<unsigned int, unsigned int> coded_video_dimensions_;
        const std::pair<unsigned int, unsigned int> display_video_dimensions_;

        const bool should_use_uniform_tiles_;
        const std::vector<unsigned int> heights_of_tiles_;
        const std::vector<unsigned int> widths_of_tiles_;

        const unsigned int pps_id_;
    };

}; //namespace stitching

#endif //HOMOMORPHIC_STITCHING_STITCHCONTEXT_H
