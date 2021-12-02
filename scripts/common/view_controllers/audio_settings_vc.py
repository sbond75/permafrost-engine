#
#  This file is part of EVERGLORY
#  Copyright (C) 2021 Eduard Permyakov 
#  All rights reserved.
#

import pf
from common.constants import *
import view_controller as vc

class AudioSettingsVC(vc.ViewController):

    def __init__(self, view):
        self.view = view
        self.view.dirty = False
        self.__og_master_volume = self.view.master_volume
        self.__og_music_volume = self.view.music_volume
        self.__og_effect_volume = self.view.effect_volume
        self.__og_mute_on_focus_loss = self.view.mute_on_focus_loss
        self.__og_music_playback_mode = self.view.music_playback_mode

    def __update_dirty_flag(self, *args):
        if self.view.master_volume != self.__og_master_volume \
        or self.view.music_volume != self.__og_music_volume \
        or self.view.effect_volume != self.__og_effect_volume \
        or self.view.mute_on_focus_loss != self.__og_mute_on_focus_loss \
        or self.view.music_playback_mode != self.__og_music_playback_mode:
            self.view.dirty = True
        else:
            self.view.dirty = False

    def __load_selection(self):
        self.view.master_volume = pf.settings_get("pf.audio.master_volume")
        self.view.music_volume = pf.settings_get("pf.audio.music_volume")
        self.view.effect_volume = pf.settings_get("pf.audio.effect_volume")
        self.view.mute_on_focus_loss = pf.settings_get("pf.audio.mute_on_focus_loss")
        self.view.music_playback_mode = pf.settings_get("pf.audio.music_playback_mode")
        self.__update_dirty_flag()

    def __on_settings_apply(self, event):
        pf.settings_set("pf.audio.master_volume", self.view.master_volume)
        self.__og_master_volume = self.view.master_volume

        pf.settings_set("pf.audio.music_volume", self.view.music_volume)
        self.__og_music_volume = self.view.music_volume

        pf.settings_set("pf.audio.effect_volume", self.view.effect_volume)
        self.__og_effect_volume = self.view.effect_volume

        pf.settings_set("pf.audio.mute_on_focus_loss", self.view.mute_on_focus_loss)
        self.__og_mute_on_focus_loss = self.view.mute_on_focus_loss

        pf.settings_set("pf.audio.music_playback_mode", self.view.music_playback_mode)
        self.__og_music_playback_mode = self.view.music_playback_mode

        self.__update_dirty_flag()
        pf.settings_flush()

    def __on_track_play(self, event):
        pf.play_music(event)

    def activate(self):
        pf.register_ui_event_handler(EVENT_SETTINGS_APPLY, AudioSettingsVC.__on_settings_apply, self)
        pf.register_ui_event_handler(EVENT_VOL_SETTING_CHANGED, AudioSettingsVC.__update_dirty_flag, self)
        pf.register_ui_event_handler(EVENT_MUTE_ON_FOCUS_LOSS_CHANGED, AudioSettingsVC.__update_dirty_flag, self)
        pf.register_ui_event_handler(EVENT_SEL_TRACK_PLAY, AudioSettingsVC.__on_track_play, self)
        self.__load_selection()

    def deactivate(self):
        pf.unregister_event_handler(EVENT_SETTINGS_APPLY, AudioSettingsVC.__on_settings_apply)
        pf.unregister_event_handler(EVENT_VOL_SETTING_CHANGED, AudioSettingsVC.__update_dirty_flag)
        pf.unregister_event_handler(EVENT_MUTE_ON_FOCUS_LOSS_CHANGED, AudioSettingsVC.__update_dirty_flag)
        pf.unregister_event_handler(EVENT_SEL_TRACK_PLAY, AudioSettingsVC.__on_track_play)

