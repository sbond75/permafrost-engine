#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import game.units.slav_warrior

class EliteSlavWarrior(game.units.slav_warrior.SlavWarrior):

    def __new__(cls, 
            path="assets/models/slav_footsoldier", 
            pfobj="slav-warrior-elite.pfobj", 
            name="EliteSlavWarrior", **kwargs):
        return super(EliteSlavWarrior, cls).__new__(cls, path, pfobj, name, **kwargs)

    def __init__(self, 
            path="assets/models/slav_footsoldier", 
            pfobj="slav-warrior-elite.pfobj", 
            name="EliteSlavWarrior", **kwargs):
        super(EliteSlavWarrior, self).__init__(path, pfobj, name,
            idle_clip = self.idle_anim(),
            max_hp = 140,
            base_dmg = 60,
            base_armour = 0.6,
            **kwargs)
        self.scale = (1.5, 1.5, 1.5)
        self.selectable = True
        self.selection_radius = 3.0
        self.vision_range = 45.0
        self.speed = 21.0

    def pr_portrait(self):
        return "assets/images/portraits/elite-warrior.png"

    def pr_icon(self):
        return "assets/images/portraits/elite-warrior.png"

    def pr_name(self):
        return "Elite Warrior"

    def pr_description(self):
        return "Skilled, disciplined, and resilient, this hardened fighter will not go down without a fight."

