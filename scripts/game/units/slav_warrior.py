#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import game.units.anim_moveable as am
import game.units.anim_combatable as ac
import game.units.presentable as pr
import game.units.status_holder as sh

class SlavWarrior(am.AnimMoveable, ac.AnimCombatable, pr.Presentable, sh.StatusHolder):

    def __new__(cls, 
            path="assets/models/slav_footsoldier", 
            pfobj="slav-warrior.pfobj", 
            name="SlavWarrior", **kwargs):
        return super(SlavWarrior, cls).__new__(cls, path, pfobj, name, **kwargs)

    def __init__(self, 
            path="assets/models/slav_footsoldier", 
            pfobj="slav-warrior.pfobj", 
            name="SlavWarrior", **kwargs):

        super(SlavWarrior, self).__init__(path, pfobj, name,
            idle_clip = kwargs.pop("idle_clip", self.idle_anim()),
            max_hp = kwargs.pop("max_hp", 100),
            base_dmg = kwargs.pop("base_dmg", 50),
            base_armour = kwargs.pop("base_armour", 0.5),
            **kwargs)
        self.scale = (1.5, 1.5, 1.5)
        self.selectable = True
        self.selection_radius = 3.0
        self.vision_range = 35.0
        self.speed = 20.0
        self.selection_priority = 3

    def idle_anim(self):
        return "Idle"

    def move_anim(self):
        return "Walking"

    def attack_anim(self): 
        return "Attack"

    def death_anim(self): 
        return "Death"

    def pr_portrait(self):
        return "assets/images/portraits/warrior.jpg"

    def pr_icon(self):
        return "assets/images/portraits/warrior.jpg"

    def pr_name(self):
        return "Warrior"

    def pr_description(self):
        return "A capable fighter, one that will not hesitate to lay down his life to perserve his ancestral way of life."

    def voicelines(self):
        return ["voicelines/warrior/%02d" % (i+1,) for i in range(0, 10)]

