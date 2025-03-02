/*****************************************************************************
 * win32.c: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004-2011 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>                 /* module_need for "video blending" */
#include <vlc_filter.h>

#include "screen.h"

#include <windows.h>


static void screen_CloseCapture(void *);
static block_t *screen_Capture(demux_t *);

typedef struct
{
    HDC hdc_src;
    HDC hdc_dst;
    BITMAPINFO bmi;
    HGDIOBJ hgdi_backup;
    POINT ptl;             /* Coordinates of the primary display's top left, when the origin
                            * is taken to be the top left of the entire virtual screen */
    size_t pitch;

    int i_fragment_size;
    int i_fragment;
    block_t *p_block;

#ifdef SCREEN_MOUSE
    filter_t *p_blend;
#endif
} screen_data_t;

#if defined(SCREEN_SUBSCREEN) || defined(SCREEN_MOUSE)
/*
 * In screen coordinates the origin is the upper-left corner of the primary
 * display, and points can have negative x/y when other displays are located
 * to the left/top of the primary.
 *
 * Windows may supply these coordinates in physical or logical units
 * depending on the version of Windows and the DPI awareness of the application.
 * I have noticed that even different interfaces of VLC (qt, rc...) can lead
 * to differences in DPI awareness. The choice of physical vs logical seems
 * to be universal though (it applies to everything we use, from GetCursorPos
 * to GetSystemMetrics and BitBlt) so we don't have to worry about anything.
 *
 * The only issue here is that it can be confusing to users when setting e.g.
 * subscreen position and dimensions. This however can be controlled by
 * disabling display scaling in the compatibility settings of the VLC executable.
 */
static inline void FromScreenCoordinates( demux_t *p_demux, POINT *p_point )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;
    p_point->x += p_data->ptl.x;
    p_point->y += p_data->ptl.y;
}
#endif

#if defined(SCREEN_SUBSCREEN)
static inline void ToScreenCoordinates( demux_t *p_demux, POINT *p_point )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;
    p_point->x -= p_data->ptl.x;
    p_point->y -= p_data->ptl.y;
}
#endif

int screen_InitCaptureGDI( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    int i_chroma, i_bits_per_pixel;

    p_sys->p_data = p_data = calloc( 1, sizeof( screen_data_t ) );
    if( !p_data )
        return VLC_ENOMEM;

    /* Get the device context for the whole screen */
    p_data->hdc_src = CreateDC( TEXT("DISPLAY"), NULL, NULL, NULL );
    if( !p_data->hdc_src )
    {
        msg_Err( p_demux, "cannot get device context" );
        free( p_data );
        return VLC_EGENERIC;
    }

    p_data->hdc_dst = CreateCompatibleDC( p_data->hdc_src );
    if( !p_data->hdc_dst )
    {
        msg_Err( p_demux, "cannot get compat device context" );
        ReleaseDC( 0, p_data->hdc_src );
        free( p_data );
        return VLC_EGENERIC;
    }

    i_bits_per_pixel = GetDeviceCaps( p_data->hdc_src, BITSPIXEL );
    switch( i_bits_per_pixel )
    {
    case 8: /* FIXME: set the palette */
        i_chroma = VLC_CODEC_RGB233; break;
    case 16:    /* Yes it is really 15 bits (when using BI_RGB) */
        i_chroma = VLC_CODEC_BGR555LE; break;
    case 24:
        i_chroma = VLC_CODEC_RGB24; break;
    case 32:
        i_chroma = VLC_CODEC_BGRX; break;
    default:
        msg_Err( p_demux, "unknown screen depth %i", i_bits_per_pixel );
        DeleteDC( p_data->hdc_dst );
        ReleaseDC( 0, p_data->hdc_src );
        free( p_data );
        return VLC_EGENERIC;
    }

    int screen_width  = GetSystemMetrics( SM_CXVIRTUALSCREEN );
    int screen_height = GetSystemMetrics( SM_CYVIRTUALSCREEN );
    es_format_Init( &p_sys->fmt, VIDEO_ES, i_chroma );
    video_format_Setup( &p_sys->fmt.video, i_chroma, screen_width, screen_height,
                        screen_width, screen_width, 1, 1 );
    p_sys->fmt.video.transfer         = TRANSFER_FUNC_SRGB;
    p_sys->fmt.video.color_range      = COLOR_RANGE_FULL;

    p_data->pitch = ( ( ( screen_width * i_bits_per_pixel ) + 31 ) & ~31 ) >> 3;

    p_data->ptl.x = - GetSystemMetrics( SM_XVIRTUALSCREEN );
    p_data->ptl.y = - GetSystemMetrics( SM_YVIRTUALSCREEN );

    static const struct screen_capture_operations ops = {
        screen_Capture, screen_CloseCapture,
    };
    p_sys->ops = &ops;

    return VLC_SUCCESS;
}

void screen_CloseCapture( void *opaque )
{
    screen_data_t *p_data = opaque;
    if( p_data->p_block ) block_Release( p_data->p_block );

    if( p_data->hgdi_backup)
        SelectObject( p_data->hdc_dst, p_data->hgdi_backup );

    DeleteDC( p_data->hdc_dst );
    ReleaseDC( 0, p_data->hdc_src );

#ifdef SCREEN_MOUSE
    if( p_data->p_blend )
    {
        filter_Close( p_data->p_blend );
        module_unneed( p_data->p_blend, p_data->p_blend->p_module );
        vlc_object_delete(p_data->p_blend);
    }
#endif

    free( p_data );
}

struct block_sys_t
{
    block_t self;
    HBITMAP hbmp;
};

static void CaptureBlockRelease( block_t *p_block )
{
    struct block_sys_t *p_sys = container_of(p_block, struct block_sys_t, self);
    DeleteObject( p_sys->hbmp );
    free( p_block );
}

static const struct vlc_block_callbacks CaptureBlockCallbacks =
{
    CaptureBlockRelease,
};

static block_t *CaptureBlockNew( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;
    struct block_sys_t *p_block;
    void *p_buffer;
    int i_buffer;
    HBITMAP hbmp;

    if( p_data->bmi.bmiHeader.biSize == 0 )
    {
        int i_val;
        /* Create the bitmap info header */
        p_data->bmi.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        p_data->bmi.bmiHeader.biWidth         = p_sys->fmt.video.i_width;
        p_data->bmi.bmiHeader.biHeight        = - p_sys->fmt.video.i_height;
        p_data->bmi.bmiHeader.biPlanes        = 1;
        p_data->bmi.bmiHeader.biBitCount      = p_data->pitch * 8 / p_sys->fmt.video.i_width;
        p_data->bmi.bmiHeader.biCompression   = BI_RGB;
        p_data->bmi.bmiHeader.biSizeImage     = 0;
        p_data->bmi.bmiHeader.biXPelsPerMeter = 0;
        p_data->bmi.bmiHeader.biYPelsPerMeter = 0;
        p_data->bmi.bmiHeader.biClrUsed       = 0;
        p_data->bmi.bmiHeader.biClrImportant  = 0;

        i_val = var_CreateGetInteger( p_demux, "screen-fragment-size" );
        p_data->i_fragment_size = i_val > 0 ? i_val : (int)p_sys->fmt.video.i_height;
        p_data->i_fragment_size = i_val > (int)p_sys->fmt.video.i_height ?
                                            (int)p_sys->fmt.video.i_height :
                                            p_data->i_fragment_size;
        p_sys->f_fps *= (p_sys->fmt.video.i_height/p_data->i_fragment_size);
        p_sys->i_incr = vlc_tick_rate_duration( p_sys->f_fps );
        p_data->i_fragment = 0;
        p_data->p_block = 0;
    }


    /* Create the bitmap storage space */
    hbmp = CreateDIBSection( p_data->hdc_dst, &p_data->bmi, DIB_RGB_COLORS,
                             &p_buffer, NULL, 0 );
    if( !hbmp || !p_buffer )
    {
        msg_Err( p_demux, "cannot create bitmap" );
        goto error;
    }

    /* Select the bitmap into the compatible DC */
    if( !p_data->hgdi_backup )
        p_data->hgdi_backup = SelectObject( p_data->hdc_dst, hbmp );
    else
        SelectObject( p_data->hdc_dst, hbmp );

    if( !p_data->hgdi_backup )
    {
        msg_Err( p_demux, "cannot select bitmap" );
        goto error;
    }

    /* Build block */
    if( !(p_block = malloc( sizeof( struct block_sys_t ) )) )
        goto error;

    /* Fill all fields */
    i_buffer = p_data->pitch * p_sys->fmt.video.i_height;
    block_Init( &p_block->self, &CaptureBlockCallbacks, p_buffer, i_buffer );
    p_block->hbmp            = hbmp;

    return &p_block->self;

error:
    if( hbmp ) DeleteObject( hbmp );
    return NULL;
}

#ifdef SCREEN_MOUSE
static void RenderCursor( demux_t *p_demux, int i_x, int i_y,
                          uint8_t *p_dst )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;
    if( !p_sys->dst.i_planes )
        picture_Setup( &p_sys->dst, &p_sys->fmt.video );

    if( !p_sys->dst.i_planes )
        return;

    /* Bitmaps here created by CreateDIBSection: stride rounded up to the nearest DWORD */
    p_sys->dst.p[ 0 ].i_pitch = p_sys->dst.p[ 0 ].i_visible_pitch = p_data->pitch;

    if( !p_data->p_blend )
    {
        p_data->p_blend = vlc_object_create( p_demux, sizeof(filter_t) );
        if( p_data->p_blend )
        {
            es_format_Init( &p_data->p_blend->fmt_in, VIDEO_ES,
                            VLC_CODEC_RGBA );
            p_data->p_blend->fmt_in.video = p_sys->p_mouse->format;
            p_data->p_blend->fmt_out = p_sys->fmt;
            p_data->p_blend->p_module =
                module_need( p_data->p_blend, "video blending", NULL, false );
            if( !p_data->p_blend->p_module )
            {
                msg_Err( p_demux, "Could not load video blending module" );
                vlc_object_delete(p_data->p_blend);
                p_data->p_blend = NULL;
                picture_Release( p_sys->p_mouse );
                p_sys->p_mouse = NULL;
            }
            assert( p_data->p_blend->ops != NULL );
        }
    }
    if( p_data->p_blend )
    {
        p_sys->dst.p->p_pixels = p_dst;
        p_data->p_blend->ops->blend_video( p_data->p_blend,
                                        &p_sys->dst,
                                        p_sys->p_mouse,
#ifdef SCREEN_SUBSCREEN
                                        i_x-p_sys->i_left,
#else
                                        i_x,
#endif
#ifdef SCREEN_SUBSCREEN
                                        i_y-p_sys->i_top,
#else
                                        i_y,
#endif
                                        255 );
    }
}
#endif

block_t *screen_Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;

    if( !p_data->i_fragment )
    {
        if( !( p_data->p_block = CaptureBlockNew( p_demux ) ) )
        {
            msg_Warn( p_demux, "cannot get block" );
            return NULL;
        }
    }

    POINT pos;
#if defined(SCREEN_SUBSCREEN) || defined(SCREEN_MOUSE)
    GetCursorPos( &pos );
    FromScreenCoordinates( p_demux, &pos );
#endif // SCREEN_SUBSCREEN || SCREEN_MOUSE
#if defined(SCREEN_SUBSCREEN)
    if( p_sys->b_follow_mouse )
    {
        FollowMouse( p_sys, pos.x, pos.y );
    }
#endif // SCREEN_SUBSCREEN

#if defined(SCREEN_SUBSCREEN)
    POINT top_left = { p_sys->i_left, p_sys->i_top };
    ToScreenCoordinates( p_demux, &top_left );
#else // !SCREEN_SUBSCREEN
    POINT top_left = { 0, 0 };
#endif // !SCREEN_SUBSCREEN

    if( !BitBlt( p_data->hdc_dst, 0,
                 p_data->i_fragment * p_data->i_fragment_size,
                 p_sys->fmt.video.i_width, p_data->i_fragment_size,
                 p_data->hdc_src, top_left.x, top_left.y +
                 p_data->i_fragment * p_data->i_fragment_size,
                 SRCCOPY | CAPTUREBLT ) )
    {
        msg_Err( p_demux, "error during BitBlt()" );
        return NULL;
    }

    p_data->i_fragment++;

    if( !( p_data->i_fragment %
           (p_sys->fmt.video.i_height/p_data->i_fragment_size) ) )
    {
        block_t *p_block = p_data->p_block;
        p_data->i_fragment = 0;
        p_data->p_block = 0;

#ifdef SCREEN_MOUSE
        if( p_sys->p_mouse )
        {
            POINT pos;

            GetCursorPos( &pos );
            FromScreenCoordinates( p_demux, &pos );
            RenderCursor( p_demux, pos.x, pos.y,
                          p_block->p_buffer );
        }
#endif

        return p_block;
    }

    return NULL;
}
