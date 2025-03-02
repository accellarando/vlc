/*****************************************************************************
 * playlist/control.c
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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
# include "config.h"
#endif

#include "ctype.h"
#include "control.h"

#include "item.h"
#include "notify.h"
#include "playlist.h"
#include "player.h"
#include "vlc_fs.h"
#include "vlc_interface.h"
#include "vlc_url.h"
#include "vlc_vector.h"

static void
vlc_playlist_PlaybackOrderChanged(vlc_playlist_t *playlist)
{
    if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
    {
        /* randomizer is expected to be empty at this point */
        assert(randomizer_Count(&playlist->randomizer) == 0);
        if (playlist->items.size)
            randomizer_Add(&playlist->randomizer, playlist->items.data,
                           playlist->items.size);

        bool loop = playlist->repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
        randomizer_SetLoop(&playlist->randomizer, loop);
    }
    else
        /* we don't use the randomizer anymore */
        randomizer_Clear(&playlist->randomizer);

    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_Notify(playlist, on_playback_order_changed, playlist->order);
    vlc_playlist_state_NotifyChanges(playlist, &state);

    char const *state_text = NULL;
    switch (playlist->order)
    {
        case VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL:
            state_text = N_("Off");
            break;
        case VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM:
            state_text = N_("On");
            break;
    }
    vlc_player_osd_Message(playlist->player,
                           _("Random: %s"), vlc_gettext(state_text));
    /* vlc_player_osd_Message() does nothing in tests */
    VLC_UNUSED(state_text);
}

static void
vlc_playlist_PlaybackRepeatChanged(vlc_playlist_t *playlist)
{
    if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
    {
        bool loop = playlist->repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
        randomizer_SetLoop(&playlist->randomizer, loop);
    }

    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_Notify(playlist, on_playback_repeat_changed, playlist->repeat);
    vlc_playlist_state_NotifyChanges(playlist, &state);

    char const *state_text = NULL;
    switch (playlist->repeat)
    {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_NONE:
            state_text = N_("Off");
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            state_text = N_("All");
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            state_text = N_("One");
            break;
    }
    vlc_player_osd_Message(playlist->player,
                           _("Loop: %s"), vlc_gettext(state_text));
    /* vlc_player_osd_Message() does nothing in tests */
    VLC_UNUSED(state_text);
}

enum vlc_playlist_playback_repeat
vlc_playlist_GetPlaybackRepeat(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    return playlist->repeat;
}

enum vlc_playlist_playback_order
vlc_playlist_GetPlaybackOrder(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    return playlist->order;
}

void
vlc_playlist_SetPlaybackRepeat(vlc_playlist_t *playlist,
                               enum vlc_playlist_playback_repeat repeat)
{
    vlc_playlist_AssertLocked(playlist);

    if (playlist->repeat == repeat)
        return;

    playlist->repeat = repeat;
    vlc_playlist_PlaybackRepeatChanged(playlist);
}

void
vlc_playlist_SetPlaybackOrder(vlc_playlist_t *playlist,
                              enum vlc_playlist_playback_order order)
{
    vlc_playlist_AssertLocked(playlist);

    if (playlist->order == order)
        return;

    playlist->order = order;
    vlc_playlist_PlaybackOrderChanged(playlist);
}

int
vlc_playlist_SetCurrentMedia(vlc_playlist_t *playlist, ssize_t index)
{
    vlc_playlist_AssertLocked(playlist);

    input_item_t *media = index != -1
                        ? playlist->items.data[index]->media
                        : NULL;
    return vlc_player_SetCurrentMedia(playlist->player, media);
}

static inline bool
vlc_playlist_NormalOrderHasPrev(vlc_playlist_t *playlist)
{
    if (playlist->current == -1)
        return false;

    if (playlist->repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL)
        return playlist->items.size > 0;

    return playlist->current > 0;
}

static inline size_t
vlc_playlist_NormalOrderGetPrevIndex(vlc_playlist_t *playlist)
{
    switch (playlist->repeat)
    {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_NONE:
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            return playlist->current - 1;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            if (playlist->current == 0)
                return playlist->items.size - 1;
            return playlist->current - 1;
        default:
            vlc_assert_unreachable();
    }
}

static inline bool
vlc_playlist_NormalOrderHasNext(vlc_playlist_t *playlist)
{
    if (playlist->repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL)
        return playlist->items.size > 0;

    /* also works if current == -1 or playlist->items.size == 0 */
    return playlist->current < (ssize_t) playlist->items.size - 1;
}

static inline size_t
vlc_playlist_NormalOrderGetNextIndex(vlc_playlist_t *playlist)
{
    switch (playlist->repeat)
    {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_NONE:
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            assert(playlist->current < (ssize_t) playlist->items.size - 1);
            return playlist->current + 1;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            assert(playlist->items.size != 0);
            return (playlist->current + 1) % playlist->items.size;
        default:
            vlc_assert_unreachable();
    }
}

static inline bool
vlc_playlist_RandomOrderHasPrev(vlc_playlist_t *playlist)
{
    return randomizer_HasPrev(&playlist->randomizer);
}

static inline size_t
vlc_playlist_RandomOrderGetPrevIndex(vlc_playlist_t *playlist)
{
    vlc_playlist_item_t *prev = randomizer_PeekPrev(&playlist->randomizer);
    assert(prev);
    ssize_t index = vlc_playlist_IndexOf(playlist, prev);
    assert(index != -1);
    return (size_t) index;
}

static inline bool
vlc_playlist_RandomOrderHasNext(vlc_playlist_t *playlist)
{
    if (playlist->repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL)
        return playlist->items.size > 0;
    return randomizer_HasNext(&playlist->randomizer);
}

static inline size_t
vlc_playlist_RandomOrderGetNextIndex(vlc_playlist_t *playlist)
{
    vlc_playlist_item_t *next = randomizer_PeekNext(&playlist->randomizer);
    assert(next);
    ssize_t index = vlc_playlist_IndexOf(playlist, next);
    assert(index != -1);
    return (size_t) index;
}

static size_t
vlc_playlist_GetPrevIndex(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    switch (playlist->order)
    {
        case VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL:
            return vlc_playlist_NormalOrderGetPrevIndex(playlist);
        case VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM:
            return vlc_playlist_RandomOrderGetPrevIndex(playlist);
        default:
            vlc_assert_unreachable();
    }
}

static size_t
vlc_playlist_GetNextIndex(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    switch (playlist->order)
    {
        case VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL:
            return vlc_playlist_NormalOrderGetNextIndex(playlist);
        case VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM:
            return vlc_playlist_RandomOrderGetNextIndex(playlist);
        default:
            vlc_assert_unreachable();
    }
}

bool
vlc_playlist_ComputeHasPrev(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    switch (playlist->order)
    {
        case VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL:
            return vlc_playlist_NormalOrderHasPrev(playlist);
        case VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM:
            return vlc_playlist_RandomOrderHasPrev(playlist);
        default:
            vlc_assert_unreachable();
    }
}

bool
vlc_playlist_ComputeHasNext(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    switch (playlist->order)
    {
        case VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL:
            return vlc_playlist_NormalOrderHasNext(playlist);
        case VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM:
            return vlc_playlist_RandomOrderHasNext(playlist);
        default:
            vlc_assert_unreachable();
    }
}

ssize_t
vlc_playlist_GetCurrentIndex(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    return playlist->current;
}

static void
vlc_playlist_SetCurrentIndex(vlc_playlist_t *playlist, ssize_t index)
{
    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    playlist->current = index;
    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_state_NotifyChanges(playlist, &state);
}

bool
vlc_playlist_HasPrev(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    return playlist->has_prev;
}

bool
vlc_playlist_HasNext(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    return playlist->has_next;
}

int
vlc_playlist_Prev(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);

    if (!vlc_playlist_ComputeHasPrev(playlist))
        return VLC_EGENERIC;

    ssize_t index = vlc_playlist_GetPrevIndex(playlist);
    assert(index != -1);

    int ret = vlc_playlist_SetCurrentMedia(playlist, index);
    if (ret != VLC_SUCCESS)
        return ret;

    if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
    {
        /* mark the item as selected in the randomizer */
        vlc_playlist_item_t *selected = randomizer_Prev(&playlist->randomizer);
        assert(selected == playlist->items.data[index]);
        VLC_UNUSED(selected);
    }

    vlc_playlist_SetCurrentIndex(playlist, index);
    vlc_player_osd_Message(playlist->player, _("Previous"));
    return VLC_SUCCESS;
}

int
vlc_playlist_Next(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);

    ssize_t index = (ssize_t)vlc_playlist_GetCurrentIndex(playlist);

    /* if we're at the end of the playlist, pick a file instead */
    if (!vlc_playlist_ComputeHasNext(playlist) || index == (ssize_t)(playlist->items.size - 1)){
        index++;
        input_item_t* next_file = vlc_playlist_GetNextFile(playlist);
        if (next_file == NULL){
            return VLC_EGENERIC;
        }
        vlc_playlist_AppendOne(playlist, next_file);
        vlc_playlist_SetCurrentMedia(playlist, index);
        vlc_playlist_SetCurrentIndex(playlist, index);
    }
    else{

        int ret = vlc_playlist_SetCurrentMedia(playlist, index);
        if (ret != VLC_SUCCESS)
            return ret;

        if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
        {
            /* mark the item as selected in the randomizer */
            vlc_playlist_item_t *selected = randomizer_Next(&playlist->randomizer);
            assert(selected == playlist->items.data[index]);
            VLC_UNUSED(selected);
        }

        vlc_playlist_SetCurrentIndex(playlist, index);
    }

    vlc_player_osd_Message(playlist->player, _("Next"));
    return VLC_SUCCESS;
}

int
vlc_playlist_GoTo(vlc_playlist_t *playlist, ssize_t index)
{
    vlc_playlist_AssertLocked(playlist);
    assert(index == -1 || (size_t) index < playlist->items.size);

    int ret = vlc_playlist_SetCurrentMedia(playlist, index);
    if (ret != VLC_SUCCESS)
        return ret;

    if (index != -1 && playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
    {
        vlc_playlist_item_t *item = playlist->items.data[index];
        randomizer_Select(&playlist->randomizer, item);
    }

    vlc_playlist_SetCurrentIndex(playlist, index);
    return VLC_SUCCESS;
}

static ssize_t
vlc_playlist_GetNextMediaIndex(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    if (playlist->repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT)
        return playlist->current;
    if (!vlc_playlist_ComputeHasNext(playlist))
        return -1;
    return (ssize_t)vlc_playlist_GetNextIndex(playlist);
}

typedef struct VLC_VECTOR(char*) vec_char_t;

/* Compare function for mixed lexicographic and natural sorting */
static int natural_compare(const void *a, const void *b) {
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;

    while (*str_a && *str_b) {
        /* If both characters are digits, compare them as numbers */
        if (isdigit(*str_a) && isdigit(*str_b)) {
            int num_a = 0, num_b = 0;

            while (*str_a && isdigit(*str_a)) {
                num_a = num_a * 10 + (*str_a - '0');
                str_a++;
            }

            while (*str_b && isdigit(*str_b)) {
                num_b = num_b * 10 + (*str_b - '0');
                str_b++;
            }

            /* Compare numeric values */
            if (num_a != num_b) {
                return num_a - num_b;
            }
        } else {
            /* Compare characters lexicographically */
            if (*str_a != *str_b) {
                return *str_a - *str_b;
            }

            str_a++;
            str_b++;
        }
    }

    /* Handle the case where one string is a prefix of the other */
    return *str_a - *str_b;
}

static bool
vlc_playlist_IsSupportedExtension(char* ext, const char* supported){
    char *supported_extensions = strdup(supported);

    /* Tokenize the supported extensions */
    char *token = strtok((char *)supported_extensions, ";");
    while (token != NULL) {
        token++; // remove the leading *
        if (strcmp(ext, token) == 0) {
            return true;
        }
        token = strtok(NULL, ";");
    }

    return false; 
}

input_item_t *
vlc_playlist_GetNextFile(vlc_playlist_t *playlist){
	vlc_playlist_AssertLocked(playlist);

    /* get files in the most recent media's directory */
    input_item_t* current = vlc_player_GetCurrentMedia(playlist->player);
    if (current == NULL)
        return NULL;
    char* directory = input_item_GetURI(current);
    if (directory == NULL)
        return NULL;
    char* last_slash = strrchr(directory, '/');
    if (last_slash == NULL)
        return NULL;

    /* remove the filename from the path */
    *last_slash = '\0';

    /* remove the "file://" from the path */
    if (strncmp(directory, "file://", 7) == 0)
        directory += 7;

    // get the files in the directory
    if (directory == NULL)
        return NULL;
    DIR* dir = vlc_opendir(directory);
    if (dir == NULL)
        return NULL;
    vec_char_t *files = malloc(sizeof(vec_char_t));
    vlc_vector_init(files);
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL){
        if (entry->d_type != DT_REG)
            continue;
        char* filename = vlc_uri_fixup(entry->d_name);
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
            continue;
        // check for supported filetype
        char* extension = strrchr(filename, '.');
        if (extension == NULL)
            continue;
        if(vlc_playlist_IsSupportedExtension(extension, EXTENSIONS_MEDIA) == false)
            continue;

        vlc_vector_push(files, strdup(filename));
    }
    qsort(files->data, files->size, sizeof(char*), natural_compare);
    /* find the current file in the array */
    char* current_filename = vlc_uri_fixup(input_item_GetURI(current));
    if (current_filename == NULL)
        return NULL;
    char* current_filename_last_slash = strrchr(current_filename, '/');
    if (current_filename_last_slash == NULL)
        return NULL;
    current_filename_last_slash++;
    /* find the current file in the array */
    size_t i;
    for (i = 0; i < files->size; i++){
        if (strcmp(files->data[i], current_filename_last_slash) == 0)
            break;
    }
    /* get the next file */
    if (i == files->size - 1){ /* hit the end, no more files after */
        vlc_closedir(dir);
        free(files);
        return NULL;
    }
    else
        i++;
    char* next_filename = files->data[i];
    /* build the next file's path */
    char* next_filename_path = malloc(strlen(directory) + strlen(next_filename) + 2);
    strcpy(next_filename_path, directory);
    strcat(next_filename_path, "/");
    strcat(next_filename_path, next_filename);

    /* build the next file's media */
    input_item_t *next_media = input_item_New(next_filename_path, NULL);
    if (next_media == NULL)
        return NULL;
    char uri[strlen("file://") + strlen(next_filename_path) + 1];
    strcpy(uri, "file://");
    strcat(uri, next_filename_path);
    input_item_SetURI(next_media, uri);
    input_item_SetName(next_media, next_media->psz_uri);

    free(next_filename_path);
    free(files);
    vlc_closedir(dir);
    return next_media;
}

	input_item_t *
vlc_playlist_GetNextMedia(vlc_playlist_t *playlist)
{
	/* the playlist and the player share the lock */
	vlc_playlist_AssertLocked(playlist);

	ssize_t index = vlc_playlist_GetNextMediaIndex(playlist);
	if (index == -1)
        return NULL;

    input_item_t *media = playlist->items.data[index]->media;
    input_item_Hold(media);
    return media;
}
