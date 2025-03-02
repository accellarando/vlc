/*****************************************************************************
 * dpb_test.c:
 *****************************************************************************
 * Copyright © 2023 VideoLabs, VideoLAN and VLC authors
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#undef NDEBUG

#ifndef DPB_DEBUG
# warning "DPB_DEBUG not defined, no useful test info"
#endif

#include "dpb.h"

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

static void pic_release(picture_t *p)
{
    free(p);
}

static inline frame_info_t *withpic(frame_info_t *info, int poc)
{
    info->p_picture = calloc(1, sizeof(*info->p_picture));
    if(!info->p_picture)
        abort();
    info->p_picture->p_sys = (void *)(uint64_t)poc;
    return info;
}

static inline frame_info_t *infocopy(const frame_info_t *ref)
{
    frame_info_t *frame;
    if(ref)
        frame = malloc(2 * sizeof(*frame));
    else
        frame = calloc(2, sizeof(*frame));
    if(!frame)
        abort();
    if(ref)
    {
        *frame = *ref;
        frame->p_picture = NULL;
        frame->p_next = NULL;
    }
    return frame;
}

static void VaCheckOutput(picture_t *output, va_list ap)
{
    for(;;)
    {
        int poc = va_arg(ap, int);
        if(poc == -1)
        {
            fprintf(stderr, "no output expected\n");
            assert(output == NULL);
            break;
        }
        if(output == NULL)
        {
            fprintf(stderr, "no output, was expected %d", poc);
            abort();
        }
        int outpoc = ((uint64_t)output->p_sys);
        fprintf(stderr, "output %d, expected %d\n", outpoc, poc);
        assert(outpoc == poc);
        picture_t *next = output->p_next;
        output->p_next = NULL;
        pic_release(output);
        output = next;
    };
    assert(output == NULL);
}

static void CheckDrain(struct dpb_s *dpb, date_t *ptsdate, ...)
{
    fprintf(stderr,"drain\n");
    picture_t *output = NULL;
    picture_t **next = &output;
    while(dpb->i_size)
    {
        *next = OutputNextFrameFromDPB(dpb, ptsdate);
        if(!*next)
            break;
        next = &((*next)->p_next);
    }
    va_list args;
    va_start(args, ptsdate);
    VaCheckOutput(output, args);
    va_end(args);
}

static void CheckOutput(struct dpb_s *dpb, date_t *ptsdate, frame_info_t *info, ...)
{
    fprintf(stderr, "enqueing foc %d flush %d dpb sz %d\n", info->i_foc,
            info->b_flush, dpb->i_size);
    dpb->i_max_pics = info->i_max_pics_buffering;
    picture_t *output = NULL;
    picture_t **next = &output;
    while(info->b_flush || dpb->i_size >= dpb->i_max_pics)
    {
        *next = OutputNextFrameFromDPB(dpb, ptsdate);
        if(!*next)
            break;
        next = &((*next)->p_next);
    }
    assert(dpb->i_size < DPB_MAX_PICS);
    va_list args;
    va_start(args, info);
    VaCheckOutput(output, args);
    va_end(args);
    InsertIntoDPB(dpb, info);
}

static void CheckDPBWithFramesTest()
{
    struct dpb_s dpb = {0};
    dpb.b_strict_reorder = true;
    dpb.b_poc_based_reorder = true;
    dpb.i_fields_per_buffer = 2;

    frame_info_t info = {0};
    info.field_rate_num = 30000;
    info.field_rate_den = 1000;
    info.b_progressive = true;
    info.b_top_field_first = true;
    info.i_num_ts = 2;
    info.i_max_pics_buffering = 4;

    date_t pts;
    date_Init(&pts, info.field_rate_num, info.field_rate_den);
    date_Set(&pts, VLC_TICK_0);

    info.i_foc = 0;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.i_foc = 4;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.i_foc = 2;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.i_foc = 0;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), 0, 2, 4, -1);

    info.i_foc = 8;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.i_foc = 2;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.i_foc = 6;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.i_foc = 4;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), 0, -1);

    /* depth reduction */
    info.i_max_pics_buffering = 2;

    info.i_foc = 10;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), 2, 4, 6, -1);

    info.i_foc = 0;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), 8, 10, -1);
    assert(dpb.i_size == 1);

    CheckDrain(&dpb, &pts, 0, -1);

    assert(dpb.i_size == 0);
}

static void CheckDPBWithFieldsTest()
{
    struct dpb_s dpb = {0};
    dpb.b_strict_reorder = true;
    dpb.b_poc_based_reorder = true;
    dpb.i_fields_per_buffer = 1;

    frame_info_t info = {0};
    info.field_rate_num = 30000;
    info.field_rate_den = 1000;
    info.b_progressive = true;
    info.b_top_field_first = true;
    info.i_num_ts = 1;
    info.i_max_pics_buffering = 2;

    /* Codec stores 1 field per buffer */
    date_t pts;
    date_Init(&pts, info.field_rate_num, info.field_rate_den);
    date_Set(&pts, VLC_TICK_0);

    info.b_field = true;

    info.i_foc = 0;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.i_foc = 2;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    assert(dpb.i_stored_fields == 2);
    assert(dpb.i_size == 2);

    info.i_foc = 1;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), 0, -1);

    CheckDrain(&dpb, &pts, 1, 2, -1);

    assert(dpb.i_stored_fields == 0);
    assert(dpb.i_size == 0);

    /* Codec stores 2 fields per buffer */
    dpb.i_fields_per_buffer = 2;

    info.i_foc = 0;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.i_foc = 2;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    assert(dpb.i_stored_fields == 2);
    assert(dpb.i_size == 1);

    info.i_foc = 1;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    assert(dpb.i_stored_fields == 3);
    assert(dpb.i_size == 2);

    CheckDrain(&dpb, &pts, 0, 1, 2, -1);

    assert(dpb.i_stored_fields == 0);
    assert(dpb.i_size == 0);

    /* progressive/mbaff/field mix for fun 1 field per buffer */
    dpb.i_fields_per_buffer = 1;
    info.i_max_pics_buffering = 3;

    info.b_field = false;
    info.i_foc = 0;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.b_field = true;
    info.i_foc = 3;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    assert(dpb.i_stored_fields == 3);
    assert(dpb.i_size == 2);

    info.i_foc = 2;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    assert(dpb.i_stored_fields == 4);
    assert(dpb.i_size == 3);

    CheckDrain(&dpb, &pts, 0, 2, 3, -1);

    assert(dpb.i_stored_fields == 0);
    assert(dpb.i_size == 0);

    /* progressive/mbaff/field mix for fun 2 fields per buffer */
    dpb.i_fields_per_buffer = 2;
    info.i_max_pics_buffering = 3;

    info.b_field = false;
    info.i_foc = 0;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    info.b_field = true;
    info.i_foc = 3;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    assert(dpb.i_stored_fields == 3);
    assert(dpb.i_size == 2);

    info.i_foc = 2;
    info.i_poc = info.i_foc & ~1;
    info.b_flush = (info.i_foc == 0);
    CheckOutput(&dpb, &pts, withpic(infocopy(&info), info.i_foc), -1);

    assert(dpb.i_stored_fields == 4);
    assert(dpb.i_size == 2);

    CheckDrain(&dpb, &pts, 0, 2, 3, -1);

    assert(dpb.i_stored_fields == 0);
    assert(dpb.i_size == 0);
}

int main(void)
{
    CheckDPBWithFramesTest();
    CheckDPBWithFieldsTest();
    return 0;
}
