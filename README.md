# TASM

Prototype implementation of TASM, which is a tile-based storage manager video analytics. See the [paper](https://arxiv.org/abs/2006.02958) for more details.

# Building
`mkdir build; cd build`  
`cmake -DCMAKE_BUILD_TYPE=Debug ..`  
`make`  

# Example usage

- After building TASM, `cd <build>/python` to ensure python can find the library.
- TASM currently requires python 2.

```
import tasm

t = tasm.TASM()

# Add metadata for a video.
t.add_metadata(video, label, frame, x1, y1, x2, y2)

# Store a video without tiling.
t.store("path/to/video.mp4", "stored-name")

# Store a video with a uniform tile layout.
t.store_with_uniform_layout("path/to/video", "stored-name", num_rows, num_cols)

# Store a video with a non-uniform tile layout based on a metadata label.
# This leads to fine-grained tiles being created around the bounding boxes associated with the specified label.
# A new layout is created for each GOP. 
t.store_with_nonuniform_layout("path/to/video", "stored-name", "metadata identifier, "metadata label")

# Store with a non-uniform tile layout, but do not tile GOPs where the layout is not expected to improve query times.
# This estimation is based on the number of pixels that have to be decoded to retrieve the specified metadata label.
t.store_with_nonuniform_layout("path/to/video", "stored-name", "metadata identifier, "metadata label", False)

# Retrieve pixels associated with labels.
selection = t.select("video", "metadata identifier", "label", first_frame_inclusive, last_frame_exclusive)

# The metadata identifier does not have to be specified when it matches the name of the stored video.
selection = t.select("video", "label", first_frame_inclusive, last_frame_exclusive)

selection = t.select("video", "label", frame)
selection = t.select("video", "metadata identifier", "label", frame)

# To select all instances of the object.
selection = t.select("video", "metadata identifier", "label")

# Inspect the instances. They are not guaranteed to be returned in ascending frame order.
while True:
    instance = selection.next()
    if instance.is_empty():
        break

    width = instance.width()
    height = instance.height()
    np_array = instance.numpy_array()

    # To view the instance.
    plt.imshow(np_array); plt.show()

# To incrementally tile the video as queries are executed.
# If not specified, the metadata identifier is assumed to be the same as the stored video name.
# The threshold indicates how much regret must accumulate before re-tiling a GOP. By default, its
# value is 1.0, meaning that the estimated reduction in decoding time must exceed the estimated cost
# of re-encoding the GOP with the new layout.
t.activate_regret_based_tiling("video")
t.activate_regret_based_tiling("video", "metadata identifier")
t.activate_regret_based_tiling("video", "metadata identifier", threshold)
    
< perform selections >

# Re-tile any GOPs that have accumulated sufficient regret.
t.retile_based_on_regret("video")

```

## Sample videos to test on
With the specific videos tested in the paper listed.
- [Netflix Public Dataset](https://github.com/Netflix/vmaf/blob/master/resource/doc/datasets.md)
    - BirdsInCage
    - CrowdRun
    - ElFuente1
    - ElFuente2
    - OldtownCross
    - Seeking
    - Tennis
- [xiph](https://media.xiph.org/video/derf/)
    - touchdown_pass
    - red_kayak
    - park_joy (2K, 4K)
    - Netflix_DrivingPOV
    - Netflix_ToddlerFountain
    - Netflix_FoodMarket
    - Netflix_FoodMarket2
    - Netflix_Narrator
    - Netflix_BoxingPractice
- [Netflix Open Source](http://download.opencontent.netflix.com/?prefix=TechblogAssets/)
    - Cosmos Laundromat
    - Meridian
- [MOT16](https://motchallenge.net/data/MOT16/)
    - All except for MOT16-05 and MOT16-06, whose resolutions are too small.
- [El Fuente](https://www.cdvl.org/documents/ElFuente_summary.pdf)
- [Visual Road](https://db.cs.washington.edu/projects/visualroad/)
    - Tested on 15 minute videos with 2K and 4K resolutions from the perspective of static traffic cameras.
    - Tested on 2K videos generated from a car's POV.


## Future enhancements:
- Expose an API toggle to choose between fine-grained and coarse-grained tile layouts.
- Support H264-encoded videos. Currently only HEVC-encoded videos are encoded.
- Reduce duplicated work when retrieving pixels from decoded frames.
