#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref

import game.units.anim_moveable as am
import game.units.anim_combatable as ac
import game.units.presentable as pr
import game.units.status_holder as sh

import game.action
import game.constants as k
import game.abilities.blessing as bless

class Blagostrazh(am.AnimMoveable, ac.AnimCombatable, pr.Presentable, sh.StatusHolder):

    def __new__(cls, 
            path="assets/models/blagostrazh", 
            pfobj="blagostrazh.pfobj", 
            name="Blagostrazh", **kwargs):
        return super(Blagostrazh, cls).__new__(cls, path, pfobj, name, **kwargs)

    def __init__(self, 
            path="assets/models/blagostrazh", 
            pfobj="blagostrazh.pfobj", 
            name="Blagostrazh", **kwargs):

        super(Blagostrazh, self).__init__(path, pfobj, name,
            idle_clip = kwargs.pop("idle_clip", self.idle_anim()),
            max_hp = kwargs.pop("max_hp", 400),
            base_dmg = kwargs.pop("base_dmg", 75),
            base_armour = kwargs.pop("base_armour", 0.6),
            **kwargs)
        self.scale = (1.7, 1.7, 1.7)
        self.selectable = True
        self.selection_radius = 3.25
        self.vision_range = 50.0
        self.speed = 20.0
        self.selection_priority = 1
        self.register(k.EVENT_SPELL_CAST, Blagostrazh.__on_spell_cast, weakref.ref(self))

    def __del__(self):
        self.unregister(k.EVENT_SPELL_CAST, Blagostrazh.__on_spell_cast)
        super(Blagostrazh, self).__del__()

    def __on_spell_cast(self, event):
        if event == "Blessing":
            self.blessing_action.start_cooldown()

    def __blessing_action(self):
        bless.BlessingSpell(self).run()
        # The spell is still not cast - wait for a notification of success 
        # to start the cooldown
        return game.action.ACTION_FAIL_TOKEN

    def action(self, idx):
        if idx == (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS) - (k.ACTION_NUM_COLS):
            if getattr(self, "blessing_action", None) is not None:
                return self.blessing_action
            ret = game.action.CooldownActionDesc(
                name = "Blagostrazh's Blessing",
                icon_normal="assets/icons/blessing-icon-normal.png",
                icon_hover ="assets/icons/blessing-icon-hover.png",
                icon_active="assets/icons/blessing-icon-active.png",
                icon_disabled="assets/icons/blessing-icon-disabled.png",
                func = self.__blessing_action,
                ent = self,
                cooldown = 60,
                hotkey = pf.SDLK_b,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_RECIPE,
                    ([], {}, "Makes the target unit invulnerable for 10 seconds and heals them to full health. "
                    "Can be self-cast.", 60)))
            setattr(self, "blessing_action", ret)
            return ret
        return super(Blagostrazh, self).action(idx)

    def idle_anim(self):
        return "Idle"

    def move_anim(self):
        return "Walking"

    def attack_anim(self): 
        return "Attack"

    def death_anim(self): 
        return "Death"

    def pr_portrait(self):
        return "assets/images/portraits/blagostrazh.png"

    def pr_icon(self):
        return "assets/images/portraits/blagostrazh.png"

    def pr_name(self):
        return "Blagostrazh"

    def pr_description(self):
        return "A mysterious man, deeply focused on something unspoken and occsionally " \
            "heard muttering under his breath about \"running a long marathon\"."

    def voicelines(self):
        return ["voicelines/blagostrazh/%02d" % (i+1,) for i in range(0, 10)]

