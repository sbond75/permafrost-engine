#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import game.units.anim_moveable as am
import game.units.anim_combatable as ac
import game.units.presentable as pr
import game.units.status_holder as sh

class YagaWalker(am.AnimMoveable, ac.AnimCombatable, pr.Presentable, sh.StatusHolder):

    def __new__(cls, 
            path="assets/models/izbushka_walker", 
            pfobj="yaga-walker.pfobj", 
            name="YagaWalker", **kwargs):
        return super(YagaWalker, cls).__new__(cls, path, pfobj, name, **kwargs)

    def __init__(self, 
            path="assets/models/izbushka_walker", 
            pfobj="yaga-walker.pfobj", 
            name="YagaWalker", **kwargs):
        super(YagaWalker, self).__init__(path, pfobj, name, 
            idle_clip=self.idle_anim(), 
            max_hp = 400,
            base_dmg = 70,
            base_armour = 0.4,
            attack_range = 50.0,
            **kwargs)
        self.speed = 24.0
        self.scale = (3.75, 3.75, 3.75)
        self.selectable = True
        self.selection_radius = 7.50
        self.vision_range = 50.0
        self.selection_priority = 2

    def idle_anim(self):
        return "Idle"

    def move_anim(self):
        return "Walking"

    def attack_anim(self): 
        return "Attack"

    def death_anim(self): 
        return "Death"

    def pr_portrait(self):
        return "assets/images/portraits/yaga-walker.png"

    def pr_icon(self):
        return "assets/images/portraits/yaga-walker.png"

    def pr_name(self):
        return "Yaga Walker"

    def pr_description(self):
        return "The ancient forest guardian and her mysterious dwelling emanate unease."

    def voicelines(self):
        return ["voicelines/yaga_walker/%02d" % (i+1,) for i in range(0, 10)]

