#include "../core/utility/include/Rectangle.h"

extern "C" {
__device__
bool someRectangleContainsPoint(const lightdb::Rectangle *rectangles,
        const unsigned int rectangle_count,
        const int im_x,
        const int im_y) {
    for (unsigned int i = 0; i < rectangle_count; ++i) {
        const lightdb::Rectangle &rect = rectangles[i];
        if (rect.x <= im_x
               && rect.y <= im_y
               && rect.x + rect.width >= im_x
               && rect.y + rect.height >= im_y) {
            return true;
        }
    }
    return false;
}

__global__
void select_pixels(
        unsigned char *nv12output,
        const unsigned int height,
        const unsigned int width,
        const unsigned int pitch,
        const lightdb::Rectangle *rectangles,
        const unsigned int rectangle_count,
        const unsigned int xOffset,
        const unsigned int yOffset) {
    const int im_x = blockDim.x * blockIdx.x + threadIdx.x;
    const int im_y = blockDim.y * blockIdx.y + threadIdx.y;

    const int global_im_x = im_x + xOffset;
    const int global_im_y = im_y + yOffset;

    if (im_x < width && im_y < height) {
        const unsigned int output_luma_offset = im_x + im_y * pitch;
        const unsigned int output_luma_size = height * pitch;
        const unsigned int output_chroma_offset = output_luma_size + im_x + (im_y / 2) * pitch;

        if (!someRectangleContainsPoint(rectangles, rectangle_count, global_im_x, global_im_y)) {
            nv12output[output_luma_offset] = 0;
            nv12output[output_chroma_offset] = 128;
        }
    }
}
} // extern "C"