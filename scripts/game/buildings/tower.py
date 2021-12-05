#
#  This file is part of EVERGLORY
#  Copyright (C) 2021 Eduard Permyakov 
#  All rights reserved.
#

import pf
import game.units.stat_combatable as sc
import game.units.presentable as pr
import game.buildings.constructable as co

class Tower(co.Constructable, sc.StatCombatable, pr.Presentable):

    def __new__(cls, 
            path="assets/models/slav_tower", 
            pfobj="slav_tower.pfobj", 
            name="Tower", **kwargs):
        return super(Tower, cls).__new__(cls, path, pfobj, name, **kwargs)

    def __init__(self, 
            path="assets/models/slav_tower", 
            pfobj="slav_tower.pfobj", 
            name="Tower", **kwargs):
        super(Tower, self).__init__(path, pfobj, name, 
            max_hp = 400,
            base_dmg = 40,
            base_armour = 0.8,
            attack_range = 75.0,
            required_resources = Tower.required_resources(),
            **kwargs
        )
        self.scale = (5.0, 5.0, 5.0)
        self.selection_radius = 20.0
        self.vision_range = 50.0

    @staticmethod
    def required_resources():
        return {"Wood" : 120}

    def pr_portrait(self):
        return "assets/images/portraits/tower.jpg"

    def pr_icon(self):
        return "assets/images/portraits/tower.jpg"

    def pr_name(self):
        return "Tower"

    def pr_description(self):
        return "A defensive structure, a crucial part of well-fortified settlement."

