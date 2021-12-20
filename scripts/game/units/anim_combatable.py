#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

from abc import ABCMeta, abstractproperty
import pf
import weakref
import game.constants as k
import game.globals 
import game.action
import game.units.controllable as cont

class AnimCombatable(pf.AnimEntity, pf.CombatableEntity, cont.Controllable):
    """ 
    Mixin base that extends animated and combatable entities with behaviours for 
    playing specific animations on attack and death, as well as adds 'hold positon'
    and 'attack' actions.
        
    """
    __metaclass__ = ABCMeta

    def __init__(self, path, pfobj, name, **kwargs):
        #print(kwargs)
        super(AnimCombatable, self).__init__(path, pfobj, name, **kwargs)
        self.attacking = False
        self.dying = False
        self.register(pf.EVENT_ATTACK_START, AnimCombatable.on_attack_begin, weakref.ref(self))
        self.register(pf.EVENT_ATTACK_END, AnimCombatable.on_attack_end, weakref.ref(self))
        self.register(pf.EVENT_ENTITY_DEATH, AnimCombatable.on_death, weakref.ref(self))
        self.register(pf.EVENT_ANIM_CYCLE_FINISHED, AnimCombatable.__on_anim_finished, weakref.ref(self))

    def __del__(self):
        self.unregister(pf.EVENT_ENTITY_DEATH, AnimCombatable.on_death)
        self.unregister(pf.EVENT_ATTACK_END, AnimCombatable.on_attack_end)
        self.unregister(pf.EVENT_ATTACK_START, AnimCombatable.on_attack_begin)
        self.unregister(pf.EVENT_ANIM_CYCLE_FINISHED, AnimCombatable.__on_anim_finished)
        super(AnimCombatable, self).__del__()

    @abstractproperty
    def attack_anim(self): 
        """ Name of animation clip that should be played when attacking """
        pass

    @abstractproperty
    def death_anim(self): 
        """ Name of animation clip that should be played on death """
        pass

    def on_attack_begin(self, event):
        assert not self.attacking
        self.attacking = True
        self.play_anim(self.attack_anim())

    def on_attack_end(self, event):
        assert self.attacking
        self.attacking = False
        self.play_anim(self.idle_anim())

    def on_death(self, event):
        self.play_anim(self.death_anim(), mode=pf.ANIM_MODE_ONCE)
        self.attacking = False
        self.dying = True
        self.register(pf.EVENT_ANIM_CYCLE_FINISHED, AnimCombatable.on_death_anim_finish, self)

    def on_death_anim_finish(self, event):
        self.unregister(pf.EVENT_ANIM_CYCLE_FINISHED, AnimCombatable.on_death_anim_finish)
        print(self)
        print(game.globals.scene_objs)
        try:
            game.globals.scene_objs.remove(self)
        except:
            import traceback
            traceback.print_exc()
            pass # TODO: temp

    def action(self, idx):
        if idx == 2:
            return game.action.ActionDesc(
                name = "Hold Position",
                icon_normal="assets/icons/hold-position-command-normal.png",
                icon_hover ="assets/icons/hold-position-command-hover.png",
                icon_active="assets/icons/hold-position-command-active.png",
                func = AnimCombatable.__hold_position_action,
                ent = self,
                hotkey = pf.SDLK_h,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Order all selected units to stand ground at their current location, engaging in combat "
                    "only if enemies are within their attack range and not leaving their post to pursue.",)))
        if idx == 3:
            return game.action.ActionDesc(
                name = "Attack",
                icon_normal="assets/icons/attack-move-command-normal.png",
                icon_hover ="assets/icons/attack-move-command-active.png",
                icon_active="assets/icons/attack-move-command-hover.png",
                func = AnimCombatable.__attack_action,
                ent = self,
                hotkey = pf.SDLK_a,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Order all selected units to travel to the targeted location, attacking any enemies "
                    "they encounter along the way. When targeting a specific unit or building, other "
                    "enemies will be ignored.",)))
        return super(AnimCombatable, self).action(idx)

    def __on_anim_finished(self, event):
        if not self.attacking:
            return
        if self.attack_range == 0:
            clip = ["sword-strike-01", "sword-strike-02", "sword-strike-03", "sword-strike-04", "sword-strike-05"][pf.rand(5)]
            pf.play_effect(clip, self.pos)
        else:
            pf.play_effect("bow-arrow", self.pos)

    @classmethod
    def __attack_action(cls):
        pf.set_attack_on_left_click()

    @classmethod
    def __hold_position_action(cls):
        for ent in pf.get_unit_selection():
            if isinstance(ent, pf.CombatableEntity):
                ent.hold_position()

