/*****************************************************************************
 * libvlc_events.h:  libvlc_events external API structure
 *****************************************************************************
 * Copyright (C) 1998-2010 VLC authors and VideoLAN
 *
 * Authors: Filippo Carone <littlejohn@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#ifndef LIBVLC_EVENTS_H
#define LIBVLC_EVENTS_H 1

# include <vlc/libvlc.h>
# include <vlc/libvlc_picture.h>
# include <vlc/libvlc_media_track.h>
# include <vlc/libvlc_media.h>

/**
 * \file
 * This file defines libvlc_event external API
 */

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

typedef struct libvlc_renderer_item_t libvlc_renderer_item_t;
typedef struct libvlc_title_description_t libvlc_title_description_t;
typedef struct libvlc_picture_t libvlc_picture_t;
typedef struct libvlc_picture_list_t libvlc_picture_list_t;
typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_media_list_t libvlc_media_list_t;

/**
 * \ingroup libvlc_event
 * @{
 */

/**
 * Event types
 */
enum libvlc_event_e {
    /* Append new event types at the end of a category.
     * Do not remove, insert or re-order any entry.
     */

    /**
     * Metadata of a \link #libvlc_media_t media item\endlink changed
     */
    libvlc_MediaMetaChanged=0,
    /**
     * Subitem was added to a \link #libvlc_media_t media item\endlink
     * \see libvlc_media_subitems()
     */
    libvlc_MediaSubItemAdded,
    /**
     * Duration of a \link #libvlc_media_t media item\endlink changed
     * \see libvlc_media_get_duration()
     */
    libvlc_MediaDurationChanged,
    /**
     * Parsing state of a \link #libvlc_media_t media item\endlink changed
     * \see libvlc_media_parse_request(),
     *      libvlc_media_get_parsed_status(),
     *      libvlc_media_parse_stop()
     */
    libvlc_MediaParsedChanged,

    /* Removed: libvlc_MediaFreed, */
    /* Removed: libvlc_MediaStateChanged */

    /**
     * Subitem tree was added to a \link #libvlc_media_t media item\endlink
     */
    libvlc_MediaSubItemTreeAdded = libvlc_MediaParsedChanged + 3,
    /**
     * A thumbnail generation for this \link #libvlc_media_t media \endlink completed.
     * \see libvlc_media_thumbnail_request_by_time()
     * \see libvlc_media_thumbnail_request_by_pos()
     */
    libvlc_MediaThumbnailGenerated,
    /**
     * One or more embedded thumbnails were found during the media preparsing
     * The user can hold these picture(s) using libvlc_picture_retain if they
     * wish to use them
     */
    libvlc_MediaAttachedThumbnailsFound,

    libvlc_MediaPlayerMediaChanged=0x100,
    libvlc_MediaPlayerNothingSpecial,
    libvlc_MediaPlayerOpening,
    libvlc_MediaPlayerBuffering,
    libvlc_MediaPlayerPlaying,
    libvlc_MediaPlayerPaused,
    libvlc_MediaPlayerStopped,
    libvlc_MediaPlayerForward,
    libvlc_MediaPlayerBackward,
    libvlc_MediaPlayerStopping,
    libvlc_MediaPlayerEncounteredError,
    libvlc_MediaPlayerTimeChanged,
    libvlc_MediaPlayerPositionChanged,
    libvlc_MediaPlayerSeekableChanged,
    libvlc_MediaPlayerPausableChanged,
    /* libvlc_MediaPlayerTitleChanged, */
    libvlc_MediaPlayerSnapshotTaken = libvlc_MediaPlayerPausableChanged + 2,
    libvlc_MediaPlayerLengthChanged,
    libvlc_MediaPlayerVout,

    /* libvlc_MediaPlayerScrambledChanged, use libvlc_MediaPlayerProgramUpdated */

    /** A track was added, cf. media_player_es_changed in \ref libvlc_event_t.u
     * to get the id of the new track. */
    libvlc_MediaPlayerESAdded = libvlc_MediaPlayerVout + 2,
    /** A track was removed, cf. media_player_es_changed in \ref
     * libvlc_event_t.u to get the id of the removed track. */
    libvlc_MediaPlayerESDeleted,
    /** Tracks were selected or unselected, cf.
     * media_player_es_selection_changed in \ref libvlc_event_t.u to get the
     * unselected and/or the selected track ids. */
    libvlc_MediaPlayerESSelected,
    libvlc_MediaPlayerCorked,
    libvlc_MediaPlayerUncorked,
    libvlc_MediaPlayerMuted,
    libvlc_MediaPlayerUnmuted,
    libvlc_MediaPlayerAudioVolume,
    libvlc_MediaPlayerAudioDevice,
    /** A track was updated, cf. media_player_es_changed in \ref
     * libvlc_event_t.u to get the id of the updated track. */
    libvlc_MediaPlayerESUpdated,
    libvlc_MediaPlayerProgramAdded,
    libvlc_MediaPlayerProgramDeleted,
    libvlc_MediaPlayerProgramSelected,
    libvlc_MediaPlayerProgramUpdated,
    /**
     * The title list changed, call
     * libvlc_media_player_get_full_title_descriptions() to get the new list.
     */
    libvlc_MediaPlayerTitleListChanged,
    /**
     * The title selection changed, cf media_player_title_selection_changed in
     * \ref libvlc_event_t.u
     */
    libvlc_MediaPlayerTitleSelectionChanged,
    libvlc_MediaPlayerChapterChanged,
    libvlc_MediaPlayerRecordChanged,

    /**
     * A \link #libvlc_media_t media item\endlink was added to a
     * \link #libvlc_media_list_t media list\endlink.
     */
    libvlc_MediaListItemAdded=0x200,
    /**
     * A \link #libvlc_media_t media item\endlink is about to get
     * added to a \link #libvlc_media_list_t media list\endlink.
     */
    libvlc_MediaListWillAddItem,
    /**
     * A \link #libvlc_media_t media item\endlink was deleted from
     * a \link #libvlc_media_list_t media list\endlink.
     */
    libvlc_MediaListItemDeleted,
    /**
     * A \link #libvlc_media_t media item\endlink is about to get
     * deleted from a \link #libvlc_media_list_t media list\endlink.
     */
    libvlc_MediaListWillDeleteItem,
    /**
     * A \link #libvlc_media_list_t media list\endlink has reached the
     * end.
     * All \link #libvlc_media_t items\endlink were either added (in
     * case of a \ref libvlc_media_discoverer_t) or parsed (preparser).
     */
    libvlc_MediaListEndReached,

    /**
     * \deprecated No longer used.
     * This belonged to the removed libvlc_media_list_view_t
     */
    libvlc_MediaListViewItemAdded LIBVLC_DEPRECATED =0x300,
    /**
     * \deprecated No longer used.
     * This belonged to the removed libvlc_media_list_view_t
     */
    libvlc_MediaListViewWillAddItem LIBVLC_DEPRECATED,
    /**
     * \deprecated No longer used.
     * This belonged to the removed libvlc_media_list_view_t
     */
    libvlc_MediaListViewItemDeleted LIBVLC_DEPRECATED,
    /**
     * \deprecated No longer used.
     * This belonged to the removed libvlc_media_list_view_t
     */
    libvlc_MediaListViewWillDeleteItem LIBVLC_DEPRECATED,

    /**
     * Playback of a \link #libvlc_media_list_player_t media list
     * player\endlink has started.
     */
    libvlc_MediaListPlayerPlayed=0x400,

    /**
     * The current \link #libvlc_media_t item\endlink of a
     * \link #libvlc_media_list_player_t media list player\endlink
     * has changed to a different item.
     */
    libvlc_MediaListPlayerNextItemSet,

    /**
     * Playback of a \link #libvlc_media_list_player_t media list
     * player\endlink has stopped.
     */
    libvlc_MediaListPlayerStopped,

    /**
     * A new \link #libvlc_renderer_item_t renderer item\endlink was found by a
     * \link #libvlc_renderer_discoverer_t renderer discoverer\endlink.
     * The renderer item is valid until deleted.
     */
    libvlc_RendererDiscovererItemAdded=0x502,

    /**
     * A previously discovered \link #libvlc_renderer_item_t renderer item\endlink
     * was deleted by a \link #libvlc_renderer_discoverer_t renderer discoverer\endlink.
     * The renderer item is no longer valid.
     */
    libvlc_RendererDiscovererItemDeleted,

    /**
     * The current media set into the \ref libvlc_media_player_t is stopping.
     *
     * This event can be used to notify when the media callbacks, initialized
     * from \ref libvlc_media_new_callbacks, should be interrupted, and in
     * particular the \ref libvlc_media_read_cb. It can also be used to signal
     * the application state that any input resource (webserver, file mounting,
     * etc) can be discarded. Output resources still need to be active until
     * the player switches to the \ref libvlc_Stopped state.
     */
    libvlc_MediaPlayerMediaStopping,
};

/**
 * A LibVLC event
 */
typedef struct libvlc_event_t
{
    int   type; /**< Event type (see @ref libvlc_event_e) */
    void *p_obj; /**< Object emitting the event */
    union
    {
        /* media descriptor */
        struct
        {
            libvlc_meta_t meta_type;
        } media_meta_changed;
        struct
        {
            libvlc_media_t * new_child;
        } media_subitem_added;
        struct
        {
            int64_t new_duration;
        } media_duration_changed;
        struct
        {
            int new_status; /**< see @ref libvlc_media_parsed_status_t */
        } media_parsed_changed;
        struct
        {
            int new_state; /**< see @ref libvlc_state_t */
        } media_state_changed;
        struct
        {
            libvlc_picture_t* p_thumbnail;
        } media_thumbnail_generated;
        struct
        {
            libvlc_media_t * item;
        } media_subitemtree_added;
        struct
        {
            libvlc_picture_list_t* thumbnails;
        } media_attached_thumbnails_found;

        /* media instance */
        struct
        {
            float new_cache;
        } media_player_buffering;
        struct
        {
            int new_chapter;
        } media_player_chapter_changed;
        struct
        {
            double new_position;
        } media_player_position_changed;
        struct
        {
            libvlc_time_t new_time;
        } media_player_time_changed;
        struct
        {
            const libvlc_title_description_t *title;
            int index;
        } media_player_title_selection_changed;
        struct
        {
            int new_seekable;
        } media_player_seekable_changed;
        struct
        {
            int new_pausable;
        } media_player_pausable_changed;
        struct
        {
            int new_scrambled;
        } media_player_scrambled_changed;
        struct
        {
            int new_count;
        } media_player_vout;

        /* media list */
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_item_added;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_will_add_item;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_item_deleted;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_will_delete_item;

        /* media list player */
        struct
        {
            libvlc_media_t * item;
        } media_list_player_next_item_set;

        /* snapshot taken */
        struct
        {
             char* psz_filename ;
        } media_player_snapshot_taken ;

        /* Length changed */
        struct
        {
            libvlc_time_t   new_length;
        } media_player_length_changed;

        /* Extra MediaPlayer */
        struct
        {
            libvlc_media_t * new_media;
        } media_player_media_changed;

        struct
        {
            libvlc_media_t * media;
        } media_player_media_stopping;


        /* ESAdded, ESDeleted, ESUpdated */
        struct
        {
            libvlc_track_type_t i_type;
            int i_id; /**< Deprecated, use psz_id */
            /** Call libvlc_media_player_get_track_from_id() to get the track
             * description. */
            const char *psz_id;
        } media_player_es_changed;

        /* ESSelected */
        struct
        {
            libvlc_track_type_t i_type;
            const char *psz_unselected_id;
            const char *psz_selected_id;
        } media_player_es_selection_changed;

        /* ProgramAdded, ProgramDeleted, ProgramUpdated */
        struct
        {
            int i_id;
        } media_player_program_changed;

        /* ProgramSelected */
        struct
        {
            int i_unselected_id;
            int i_selected_id;
        } media_player_program_selection_changed;

        struct
        {
            float volume;
        } media_player_audio_volume;

        struct
        {
            const char *device;
        } media_player_audio_device;

        struct
        {
            bool recording;
            /** Only valid when recording ends (recording == false) */
            const char *recorded_file_path;
        } media_player_record_changed;

        struct
        {
            libvlc_renderer_item_t *item;
        } renderer_discoverer_item_added;
        struct
        {
            libvlc_renderer_item_t *item;
        } renderer_discoverer_item_deleted;
    } u; /**< Type-dependent event description */
} libvlc_event_t;


/**@} */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_EVENTS_H */
