/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

import QtQuick 2.12
import QtQuick.Templates 2.12 as T

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers

T.Control {
    id: root

    // Properties

    property bool paintOnly: false

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.ToolButton

        enabled: root.enabled || root.paintOnly
        focused: root.activeFocus
        hovered: root.cursorInside
        pressed: mouseArea.containsPress
    }

    property bool _keyOkPressed: false

    // Aliases

    property alias cursorInside: mouseArea.cursorInside

    // Settings

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
    implicitHeight: implicitBackgroundHeight + topInset + bottomInset

    scale: (_keyOkPressed || (mouseArea.pressed && cursorInside)) ? 0.95
                                                                  : 1.00

    Accessible.role: Accessible.Button
    Accessible.name: I18n.qtr("Play/Pause")
    Accessible.checkable: true
    Accessible.checked: Player.playingState !== Player.PLAYING_STATE_PAUSED
                        && Player.playingState !== Player.PLAYING_STATE_STOPPED
    Accessible.onPressAction: MainPlaylistController.togglePlayPause()
    Accessible.onToggleAction: MainPlaylistController.togglePlayPause()

    // States

    states: [
        State {
            name: "focused"
            when: visualFocus

            PropertyChanges {
                target: hoverShadow
                opacity: 0.0
            }

            PropertyChanges {
                target: focusShadow
                opacity: 1.0
            }
        },
        State {
            name: "hover"
            when: cursorInside

            PropertyChanges {
                target: hoverShadow
                opacity: 1.0
            }

            PropertyChanges {
                target: focusShadow
                opacity: 0.0
            }
        }
    ]

    transitions: Transition {
        from: ""; to: "*"
        reversible: true
        NumberAnimation {
            properties: "opacity"
            easing.type: Easing.InOutSine
            duration: VLCStyle.duration_veryShort
        }
    }

    // Keys

    Keys.onPressed: {
        if (KeyHelper.matchOk(event) ) {
            if (!event.isAutoRepeat) {
                _keyOkPressed = true
                keyHoldTimer.restart()
            }
            event.accepted = true
        }
        Navigation.defaultKeyAction(event)
    }

    Keys.onReleased: {
        if (KeyHelper.matchOk(event)) {
            if (!event.isAutoRepeat) {
                _keyOkPressed = false
                keyHoldTimer.stop()
                if (Player.playingState !== Player.PLAYING_STATE_STOPPED)
                    MainPlaylistController.togglePlayPause()
            }
            event.accepted = true
        }
    }

    // Functions

    function _pressAndHoldAction() {
        _keyOkPressed = false
        MainPlaylistController.stop()
    }

    // Children

    Timer {
        id: keyHoldTimer

        interval: mouseArea.pressAndHoldInterval
        repeat: false

        Component.onCompleted: {
            triggered.connect(_pressAndHoldAction)
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent

        hoverEnabled: true

        readonly property bool cursorInside: {
            if (!containsMouse)
                return false

            const center = (width / 2)
            if (Helpers.pointInRadius( center - mouseX,
                                       center - mouseY,
                                       center )) {
                return true
            } else {
                return false
            }
        }

        onPressed: {
            if (!cursorInside) {
                mouse.accepted = false
                return
            }

            root.forceActiveFocus(Qt.MouseFocusReason)
        }

        onClicked: {
            MainPlaylistController.togglePlayPause()
            mouse.accepted = true
        }

        onPressAndHold: {
            _pressAndHoldAction()
            mouse.accepted = true
        }
    }

    contentItem: T.Label {
        text: {
            const state = Player.playingState

            if (!paintOnly
                    && state !== Player.PLAYING_STATE_PAUSED
                    && state !== Player.PLAYING_STATE_STOPPED)
                return VLCIcons.pause_filled
            else
                return VLCIcons.play_filled
        }

        color: cursorInside ? theme.accent
                            : "black"  //foreground is always black

        font.pixelSize: Math.round(parent.height / 1.7)

        font.family: VLCIcons.fontFamily

        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        Accessible.ignored: true

        Behavior on color {
            enabled: theme.initialized
            ColorAnimation {
                duration: VLCStyle.duration_veryShort
                easing.type: Easing.InOutSine
            }
        }
    }

    background: Item {
        implicitWidth: height
        implicitHeight: VLCStyle.icon_medium

        // TODO: Qt >= 5.15 use inline component for the drop shadows
        Widgets.DropShadowImage {
            id: hoverShadow

            anchors.centerIn: parent

            visible: opacity > 0
            opacity: 0

            blurRadius: VLCStyle.dp(9)
            yOffset: VLCStyle.dp(4)

            color: VLCStyle.setColorAlpha(theme.accent, 0.29)

            xRadius: parent.width
            yRadius: xRadius

            sourceSize: Qt.size(xRadius, yRadius)
        }

        Widgets.DropShadowImage {
            id: focusShadow

            anchors.centerIn: parent

            visible: opacity > 0
            opacity: 0

            blurRadius: VLCStyle.dp(14)
            yOffset: VLCStyle.dp(1)

            color: VLCStyle.setColorAlpha(theme.accent, 1.0)

            xRadius: parent.width
            yRadius: xRadius

            sourceSize: Qt.size(xRadius, yRadius)
        }

        Widgets.ScaledImage {
            anchors.fill: parent
            source: "qrc:/misc/play_button.svg"
        }
    }
}
