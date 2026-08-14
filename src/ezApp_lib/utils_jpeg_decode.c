// -----------------  NOT ANDROID - TEST CODE  ---------------------------
// xxx cleanup

#ifdef ANDROID

#include <std_hdrs.h>
    
#include <utils.h>
#include <private.h>

#include <android/imagedecoder.h>
//#include <android/log.h>
//#include <stdlib.h>
//#include <stdio.h>

/**
 * Decodes a JPEG file descriptor into a raw RGBA_8888 pixel buffer.
 * 
 * @param fd           Open file descriptor of the JPEG file.
 * @param out_width    Pointer to store the output image width.
 * @param out_height   Pointer to store the output image height.
 * @param out_pixels   Pointer to store the allocated raw pixel buffer address.
 * @return             0 on success, negative value on failure.
 */
// xxx ERROR ?  is this working
int util_decode_jpeg_to_raw(int fd, int* out_width, int* out_height, void** out_pixels) 
{
    if (fd < 0 || !out_width || !out_height || !out_pixels) {
        ERROR("Invalid arguments provided.");
        return -1;
    }

    AImageDecoder* decoder = NULL;
    int result = AImageDecoder_createFromFd(fd, &decoder);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS || decoder == NULL) {
        ERROR("Failed to create image decoder. Error code: %d", result);
        return -2;
    }

    // 1. Retrieve image metadata
    const AImageDecoderHeaderInfo* header = AImageDecoder_getHeaderInfo(decoder);
    int width = AImageDecoderHeaderInfo_getWidth(header);
    int height = AImageDecoderHeaderInfo_getHeight(header);
    
    // Force the target output format to 8888 RGBA (4 bytes per pixel)
    result = AImageDecoder_setAndroidBitmapFormat(decoder, ANDROID_BITMAP_FORMAT_RGBA_8888);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS) {
        ERROR("Failed to set output format to RGBA_8888.");
        AImageDecoder_delete(decoder);
        return -3;
    }

    // 2. Calculate memory requirements
    size_t stride = AImageDecoder_getMinimumStride(decoder);
    size_t buffer_size = stride * height;

    // 3. Allocate the raw pixel buffer
    void* pixel_buffer = malloc(buffer_size);
    if (!pixel_buffer) {
        ERROR("Failed to allocate memory for pixel buffer.");
        AImageDecoder_delete(decoder);
        return -4;
    }

    // 4. Perform the actual decode operation
    result = AImageDecoder_decodeImage(decoder, pixel_buffer, stride, buffer_size);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS) {
        ERROR("Decoding failed. Error code: %d", result);
        free(pixel_buffer);
        AImageDecoder_delete(decoder);
        return -5;
    }

    // 5. Populate output parameters and clean up resources
    *out_width = width;
    *out_height = height;
    *out_pixels = pixel_buffer;

    AImageDecoder_delete(decoder);
    return 0; // Success
}

#else

// -----------------  NOT ANDROID - TEST CODE  ---------------------------

#include <std_hdrs.h>

#include <utils.h>
#include <private.h>

int util_decode_jpeg_to_raw(int fd, int* out_width, int* out_height, void** out_pixels) 
{
    ERROR("this routine only supported on Android\n");
    return -1;
}

#endif
