/*****************************************************************************
 * VLCPlaylistTableViewCell.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCPlaylistTableCellView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"

#import "library/VLCLibraryImageCache.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlaylistItem.h"

#import "views/VLCImageView.h"

#import <vlc_configuration.h>

@implementation VLCPlaylistTableCellView

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)setRepresentsCurrentPlaylistItem:(BOOL)representsCurrentPlaylistItem
{
    _representsCurrentPlaylistItem = representsCurrentPlaylistItem;

    NSFont * const displayedFont = _representsCurrentPlaylistItem ?
        [NSFont boldSystemFontOfSize:NSFont.systemFontSize] :
        [NSFont systemFontOfSize:NSFont.systemFontSize];

    NSFont * const sublineDisplayedFont = _representsCurrentPlaylistItem ?
        [NSFont boldSystemFontOfSize:NSFont.smallSystemFontSize] :
        [NSFont systemFontOfSize:NSFont.smallSystemFontSize];

    self.mediaTitleTextField.font = displayedFont;
    self.secondaryMediaTitleTextField.font = displayedFont;
    self.durationTextField.font = displayedFont;
    self.artistTextField.font = sublineDisplayedFont;
}

- (void)setRepresentedPlaylistItem:(VLCPlaylistItem *)item
{
    [VLCLibraryImageCache thumbnailForPlaylistItem:item withCompletion:^(NSImage * const thumbnail) {
        self.audioArtworkImageView.image = thumbnail;
        self.mediaImageView.image = thumbnail;
    }];

    const BOOL validArtistString = item.artistName && item.artistName.length > 0;
    const BOOL validAlbumString = item.albumName && item.albumName > 0;

    NSString *songDetailString;

    if (validArtistString && validAlbumString) {
        songDetailString = [NSString stringWithFormat:@"%@ · %@", item.artistName, item.albumName];
    } else if (validArtistString) {
        songDetailString = item.artistName;
    }

    if (songDetailString) {
        self.mediaTitleTextField.hidden = YES;
        self.secondaryMediaTitleTextField.hidden = NO;
        self.artistTextField.hidden = NO;
        self.secondaryMediaTitleTextField.stringValue = item.title;
        self.artistTextField.stringValue = songDetailString;
        self.audioMediaTypeIndicator.hidden = NO;

        self.audioArtworkImageView.hidden = NO;
        self.mediaImageView.hidden = YES;
    } else {
        self.mediaTitleTextField.hidden = NO;
        self.secondaryMediaTitleTextField.hidden = YES;
        self.artistTextField.hidden = YES;
        self.mediaTitleTextField.stringValue = item.title;
        self.audioMediaTypeIndicator.hidden = YES;

        self.audioArtworkImageView.hidden = YES;
        self.mediaImageView.hidden = NO;
    }

    self.durationTextField.stringValue = [NSString stringWithTimeFromTicks:item.duration];

    _representedPlaylistItem = item;
}

@end
