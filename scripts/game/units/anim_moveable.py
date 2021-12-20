#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

from abc import ABCMeta, abstractproperty
import pf
import weakref
import game.constants as k
import game.units.controllable as cont
import game.action

class AnimMoveable(pf.AnimEntity, pf.MovableEntity, cont.Controllable):
    """ 
    Mixin base that extends animated entities with behaviours for playing specific
    animations on movement, as well as adds 'move' and 'stop' actions.
    """
    __metaclass__ = ABCMeta

    def __init__(self, path, pfobj, name, **kwargs):
        #print(kwargs)
        super(AnimMoveable, self).__init__(path, pfobj, name, **kwargs)
        self.moving = False
        self.register(pf.EVENT_MOTION_START, AnimMoveable.__on_motion_begin, weakref.ref(self))
        self.register(pf.EVENT_MOTION_END, AnimMoveable.__on_motion_end, weakref.ref(self))
        self.register(pf.EVENT_ANIM_CYCLE_FINISHED, AnimMoveable.__on_anim_finish, weakref.ref(self))

    def __del__(self):
        self.unregister(pf.EVENT_ANIM_CYCLE_FINISHED, AnimMoveable.__on_anim_finish)
        self.unregister(pf.EVENT_MOTION_START, AnimMoveable.__on_motion_begin)
        self.unregister(pf.EVENT_MOTION_END, AnimMoveable.__on_motion_end)
        super(AnimMoveable, self).__del__()

    def __on_anim_finish(self, event):
        if hasattr(self, "attacking") and self.attacking:
            return
        if hasattr(self, "building") and self.building:
            return
        if hasattr(self, "harvesting") and self.harvesting:
            return
        if hasattr(self, "dying") and self.dying:
            return
        if not self.moving:
            self.play_anim(self.idle_anim())

    @abstractproperty
    def idle_anim(self): 
        """ Name of animation clip that should be played when not moving """
        pass

    @abstractproperty
    def move_anim(self): 
        """ Name of animation clip that should be played when moving """
        pass

    def __on_motion_begin(self, event):
        assert not self.moving
        self.moving = True
        self.play_anim(self.move_anim())

    def __on_motion_end(self, event):
        assert self.moving
        self.moving = False
        self.play_anim(self.idle_anim())

    def action(self, idx):
        if idx == 0:
            return game.action.ActionDesc(
                name = "Move",
                icon_normal="assets/icons/move-command-normal.png",
                icon_hover ="assets/icons/move-command-hover.png",
                icon_active="assets/icons/move-command-active.png",
                func = AnimMoveable.__move_action,
                ent = self,
                hotkey = pf.SDLK_m,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Order all selected units to travel to the targeted location. You may also "
                    "simply right-click on the map (or minimap) to give this order.",)))
        if idx == 1 and super(AnimMoveable, self).action(1) is None:
            return game.action.ActionDesc(
                name = "Stop",
                icon_normal="assets/icons/stop-command-normal.png",
                icon_hover ="assets/icons/stop-command-active.png",
                icon_active="assets/icons/stop-command-hover.png",
                func = AnimMoveable.__stop_action,
                ent = self,
                hotkey = pf.SDLK_s,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Order all selected units to immediately stop any action they are "
                    "currently performing.",)))
        return super(AnimMoveable, self).action(idx)

    @classmethod
    def __move_action(cls):
        pf.set_move_on_left_click()

    @classmethod
    def __stop_action(cls):
        for ent in pf.get_unit_selection():
            ent.stop()

