/*****************************************************************************
 * mediacodec.h: Video decoder module using the Android MediaCodec API
 *****************************************************************************
 * Copyright © 2015 VLC authors and VideoLAN, VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_MEDIACODEC_H
#define VLC_MEDIACODEC_H

#include <vlc_common.h>
#include "../../video_output/android/utils.h"

typedef struct mc_api mc_api;
typedef struct mc_api_sys mc_api_sys;
typedef struct mc_api_out mc_api_out;

typedef int (*pf_MediaCodecApi_init)(mc_api*);

int MediaCodecJni_Init(mc_api*);
int MediaCodecNdk_Init(mc_api*);

#define MC_API_ERROR (-1)
#define MC_API_INFO_TRYAGAIN (-11)
#define MC_API_INFO_OUTPUT_FORMAT_CHANGED (-12)
#define MC_API_INFO_OUTPUT_BUFFERS_CHANGED (-13)

/* in sync with OMXCODEC QUIRKS */
#define MC_API_NO_QUIRKS 0
#define MC_API_QUIRKS_NEED_CSD 0x1
#define MC_API_VIDEO_QUIRKS_IGNORE_PADDING 0x2
#define MC_API_VIDEO_QUIRKS_SUPPORT_INTERLACED 0x4
#define MC_API_AUDIO_QUIRKS_NEED_CHANNELS 0x8

/* MediaCodec only QUIRKS */
#define MC_API_VIDEO_QUIRKS_ADAPTIVE 0x1000
#define MC_API_VIDEO_QUIRKS_IGNORE_SIZE 0x2000

/* cf. https://github.com/FFmpeg/FFmpeg/blob/00f5a34c9a5f0adee28aca11971918d6aca48745/libavcodec/mediacodec_wrapper.h#L348
 * cf. https://developer.android.com/reference/android/media/MediaFormat#constants_1*/
enum mc_media_format_color_range_t
{
    MC_COLOR_RANGE_UNSPECIFIED = 0x0,
    MC_COLOR_RANGE_FULL        = 0x1,
    MC_COLOR_RANGE_LIMITED     = 0x2,
};

enum mc_media_format_color_standard_t
{
    MC_COLOR_STANDARD_UNSPECIFIED  = 0x0,
    MC_COLOR_STANDARD_BT709        = 0x1,
    MC_COLOR_STANDARD_BT601_PAL    = 0x2,
    MC_COLOR_STANDARD_BT601_NTSC   = 0x4,
    MC_COLOR_STANDARD_BT2020       = 0x6,
};

enum mc_media_format_color_transfer_t
{
    MC_COLOR_TRANSFER_UNSPECIFIED = 0x0,
    MC_COLOR_TRANSFER_LINEAR      = 0x1,
    MC_COLOR_TRANSFER_SDR_VIDEO   = 0x3,
    MC_COLOR_TRANSFER_ST2084      = 0x6,
    MC_COLOR_TRANSFER_HLG         = 0x7,
};

struct mc_api_out
{
    enum {
        MC_OUT_TYPE_BUF,
        MC_OUT_TYPE_CONF,
    } type;
    bool b_eos;
    union
    {
        struct
        {
            int i_index;
            vlc_tick_t i_ts;
            const uint8_t *p_ptr;
            size_t i_size;
        } buf;
        union
        {
            struct
            {
                unsigned int width, height;
                unsigned int stride;
                unsigned int slice_height;
                int pixel_format;
                int crop_left;
                int crop_top;
                int crop_right;
                int crop_bottom;
            } video;
            struct
            {
                int channel_count;
                int channel_mask;
                int sample_rate;
            } audio;
        } conf;
    };
};

union mc_api_args
{
    struct
    {
        void *p_surface;
        void *p_jsurface;
        int i_width;
        int i_height;
        int i_angle;
        bool b_tunneled_playback;
        bool b_adaptive_playback;
        enum mc_media_format_color_transfer_t color_transfer;
        enum mc_media_format_color_range_t color_range;
        enum mc_media_format_color_standard_t color_standard;
    } video;
    struct
    {
        int i_sample_rate;
        int i_channel_count;
    } audio;
};

struct mc_api
{
    mc_api_sys *p_sys;

    /* Set before init */
    vlc_object_t *  p_obj;
    const char *    psz_mime;
    enum es_format_category_e i_cat;
    vlc_fourcc_t    i_codec;

    /* Set after configure */
    int  i_quirks;
    char *psz_name;
    bool b_support_rotation;

    bool b_started;
    bool b_direct_rendering;

    void (*clean)(mc_api *);
    int (*configure)(mc_api *, int i_profile);
    int (*start)(mc_api *, union mc_api_args *p_args);
    int (*stop)(mc_api *);
    int (*flush)(mc_api *);

    /* The Dequeue functions return:
     * - The index of the input or output buffer if >= 0,
     * - MC_API_INFO_TRYAGAIN if no buffers where dequeued during i_timeout,
     * - MC_API_INFO_OUTPUT_FORMAT_CHANGED if output format changed
     * - MC_API_INFO_OUTPUT_BUFFERS_CHANGED if buffers changed
     * - MC_API_ERROR in case of error. */
    int (*dequeue_in)(mc_api *, vlc_tick_t i_timeout);
    int (*dequeue_out)(mc_api *, vlc_tick_t i_timeout);

    /* i_index is the index returned by dequeue_in and should be >= 0
     * Returns 0 if buffer is successfully queued, or MC_API_ERROR */
    int (*queue_in)(mc_api *, int i_index, const void *p_buf, size_t i_size,
                    vlc_tick_t i_ts, bool b_config);

    /* i_index is the index returned by dequeue_out and should be >= 0,
     * MC_API_INFO_OUTPUT_FORMAT_CHANGED, or MC_API_INFO_OUTPUT_BUFFERS_CHANGED.
     * Returns 1 if p_out if valid, 0 if p_out is unchanged or MC_API_ERROR */
    int (*get_out)(mc_api *, int i_index, mc_api_out *p_out);

    /* i_index is the index returned by dequeue_out and should be >= 0 */
    int (*release_out)(mc_api *, int i_index, bool b_render);

    /* render a buffer at a specified ts */
    int (*release_out_ts)(mc_api *, int i_index, int64_t i_ts_ns);

    /* Dynamically sets the output surface
     * Returns 0 on success, or MC_API_ERROR */
    int (*set_output_surface)(mc_api*, void *p_surface, void *p_jsurface);
};

#endif
