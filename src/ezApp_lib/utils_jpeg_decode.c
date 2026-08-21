// -----------------  ANDROID  ---------------------------

#ifdef ANDROID

#include <std_hdrs.h>
    
#include <utils.h>
#include <private.h>

#include <android/imagedecoder.h>

/**
 * Decodes a JPEG file descriptor into a raw RGBA_8888 pixel buffer.
 * 
 * @param fd           Open file descriptor of the JPEG file.
 * @param out_width    Pointer to store the output image width.
 * @param out_height   Pointer to store the output image height.
 * @param out_pixels   Pointer to store the allocated raw pixel buffer address.
 * @return             0 on success, negative value on failure.
 */

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

    // Retrieve image metadata
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

    // Calculate memory requirements
    size_t stride = AImageDecoder_getMinimumStride(decoder);
    size_t buffer_size = stride * height;

    // Allocate the raw pixel buffer
    void* pixel_buffer = malloc(buffer_size);
    if (!pixel_buffer) {
        ERROR("Failed to allocate memory for pixel buffer.");
        AImageDecoder_delete(decoder);
        return -4;
    }

    // Perform the actual decode operation
    result = AImageDecoder_decodeImage(decoder, pixel_buffer, stride, buffer_size);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS) {
        ERROR("Decoding failed. Error code: %d", result);
        free(pixel_buffer);
        AImageDecoder_delete(decoder);
        return -5;
    }

    // Populate output parameters and clean up resources
    *out_width = width;
    *out_height = height;
    *out_pixels = pixel_buffer;

    // Cleanup and return success
    AImageDecoder_delete(decoder);
    return 0;
}

#else

// -----------------  NOT ANDROID - TEST CODE  ---------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <jpeglib.h>

#include <utils.h>

// Helper function to read the EXIF orientation marker from a JPEG file descriptor
static int get_exif_orientation(int fd)
{
    int orientation = 1; // Default: normal orientation
    unsigned char marker_header[4];
    
    // Duplicate and rewind a descriptor to peek at EXIF
    int peek_fd = dup(fd);
    if (peek_fd < 0) return 1;
    lseek(peek_fd, 0, SEEK_SET);

    // Read SOI (Start of Image) marker: 0xFFD8
    if (read(peek_fd, marker_header, 2) != 2 || marker_header[0] != 0xFF || marker_header[1] != 0xD8) {
        close(peek_fd);
        return 1;
    }

    // Scan through JPEG markers looking for APP1 (0xFFE1) which contains EXIF
    while (1) {
        if (read(peek_fd, marker_header, 4) != 4 || marker_header[0] != 0xFF) break;
        
        unsigned char marker = marker_header[1];
        unsigned short length = (marker_header[2] << 8) | marker_header[3];
        
        if (marker == 0xE1) { // APP1 Marker found
            unsigned char *exif_data = malloc(length - 2);
            if (!exif_data) break;
            
            if (read(peek_fd, exif_data, length - 2) == length - 2) {
                // Check for "Exif\0\0" magic string
                if (length >= 8 && exif_data[0] == 'E' && exif_data[1] == 'x' && exif_data[2] == 'i' && exif_data[3] == 'f' && exif_data[4] == '\0') {
                    // Check Endianness of TIFF header
                    int is_little_endian = (exif_data[6] == 'I' && exif_data[7] == 'I');
                    unsigned int tiff_offset = 6;
                    
                    // Get offset to first IFD
                    unsigned int ifd_offset;
                    if (is_little_endian) {
                        ifd_offset = exif_data[tiff_offset + 4] | (exif_data[tiff_offset + 5] << 8) | (exif_data[tiff_offset + 6] << 16) | (exif_data[tiff_offset + 7] << 24);
                    } else {
                        ifd_offset = (exif_data[tiff_offset + 4] << 24) | (exif_data[tiff_offset + 5] << 16) | (exif_data[tiff_offset + 6] << 8) | exif_data[tiff_offset + 7];
                    }
                    
                    unsigned int p = tiff_offset + ifd_offset;
                    if (p + 2 < length - 2) {
                        unsigned short num_entries = is_little_endian ? (exif_data[p] | (exif_data[p+1] << 8)) : ((exif_data[p] << 8) | exif_data[p+1]);
                        p += 2;
                        
                        // Loop through tags to find Orientation (0x0112)
                        for (int i = 0; i < num_entries && p + 12 < length - 2; i++, p += 12) {
                            unsigned short tag = is_little_endian ? (exif_data[p] | (exif_data[p+1] << 8)) : ((exif_data[p] << 8) | exif_data[p+1]);
                            if (tag == 0x0112) { // Orientation Tag
                                unsigned short val = is_little_endian ? (exif_data[p+8] | (exif_data[p+9] << 8)) : ((exif_data[p+8] << 8) | exif_data[p+9]);
                                if (val >= 1 && val <= 8) orientation = val;
                                break;
                            }
                        }
                    }
                }
            }
            free(exif_data);
            break;
        } else if (marker == 0xDA) { // SOS (Start of Scan) - Image data starts, stop searching
            break;
        } else {
            // Skip this marker's payload
            lseek(peek_fd, length - 2, SEEK_CUR);
        }
    }
    
    close(peek_fd);
    return orientation;
}

// Convert a jpg file to raw 32 bit RGBA pixel format, correcting for EXIF orientation rotation.
int util_decode_jpeg_to_raw(int fd, int *out_width, int *out_height, void **out_pixels)
{
    if (fd < 0 || !out_width || !out_height || !out_pixels) {
        return -1;
    }

    // Detect if the image has an embedded rotation flag before decoding
    int orientation = get_exif_orientation(fd);

    // Prepare libjpeg structures
    int fd_copy = dup(fd);
    if (fd_copy < 0) return -1;
    lseek(fd_copy, 0, SEEK_SET); // Reset pointer for libjpeg
    
    FILE *infile = fdopen(fd_copy, "rb");
    if (!infile) {
        close(fd_copy);
        return -1;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return -1;
    }

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    int src_w = cinfo.output_width;
    int src_h = cinfo.output_height;
    int row_stride = src_w * cinfo.output_components;

    // Determine final output dimensions based on EXIF tag
    // Orientation values 5, 6, 7, 8 mean the image is rotated 90 or 270 degrees.
    int flip_dimensions = (orientation >= 5 && orientation <= 8);
    int dest_w = flip_dimensions ? src_h : src_w;
    int dest_h = flip_dimensions ? src_w : src_h;

    unsigned char *rgba_pixels = (unsigned char *)malloc(dest_w * dest_h * 4);
    if (!rgba_pixels) {
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return -1;
    }

    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    // Decompress and map native pixels into their corrected final rotation coordinates
    int src_y = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        unsigned char *rgb_row = buffer[0];

        for (int src_x = 0; src_x < src_w; src_x++) {
            int dest_x = 0;
            int dest_y = 0;

            // Map source pixel coordinates (src_x, src_y) to visual output coordinates (dest_x, dest_y)
            switch (orientation) {
                case 3: // 180 degrees
                    dest_x = src_w - 1 - src_x;
                    dest_y = src_h - 1 - src_y;
                    break;
                case 6: // 90 degrees clockwise (Most common for portrait cell phone photos)
                    dest_x = src_h - 1 - src_y;
                    dest_y = src_x;
                    break;
                case 8: // 270 degrees clockwise
                    dest_x = src_y;
                    dest_y = src_w - 1 - src_x;
                    break;
                default: // Case 1 (Normal) or unsupported mirror flags (2, 4, 5, 7)
                    dest_x = src_x;
                    dest_y = src_y;
                    break;
            }

            unsigned char *rgba_pixel = &rgba_pixels[(dest_y * dest_w + dest_x) * 4];
            rgba_pixel[0] = rgb_row[src_x * 3 + 0]; // R
            rgba_pixel[1] = rgb_row[src_x * 3 + 1]; // G
            rgba_pixel[2] = rgb_row[src_x * 3 + 2]; // B
            rgba_pixel[3] = 255;                    // A
        }
        src_y++;
    }

    // Clean up libjpeg and file structures;
    // fclose(infile) also closes fd_copy
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    // Set output parameters
    *out_width = dest_w;
    *out_height = dest_h;
    *out_pixels = (void *)rgba_pixels;

    // success
    return 0;
}

#endif
