#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref
import common.notifier_region as nr

import game.buildings.storage as st
import game.buildings.constructable as co
import game.action as action
import game.constants as k
import game.unit_training as ut

import game.units.stat_combatable as sc
import game.units.presentable as pr

import game.units.chicken
import game.units.slav_peasant 
import game.units.slav_warrior
import game.units.elite_slav_warrior
import game.units.yaga_walker

class Barracks(co.Constructable, st.Storage, sc.StatCombatable, pr.Presentable):

    def __new__(cls,
            path="assets/models/slav_barracks",
            pfobj="slav_barracks.pfobj",
            name="Barracks", **kwargs):
        return super(Barracks, cls).__new__(cls, path, pfobj, name, **kwargs)

    def __init__(self,
            path="assets/models/slav_barracks",
            pfobj="slav_barracks.pfobj",
            name="Barracks", **kwargs):
        self.scale = (5.0, 5.0, 5.0)
        self.selection_radius = 30.0
        self.vision_range = 50.0
        self.set_capacity("Gold", 100)
        self.set_capacity("Iron", 100)
        self.set_capacity("Wood", 100)
        self.__region = nr.NotifierRegion(
            self,
            k.EVENT_REGION_CONTENTS_CHANGED,
            type=pf.REGION_RECTANGLE, 
            name="Barracks." + str(self.uid),
            position=(self.pos[0] + self.bounds[0]/2.0 + 15, self.pos[2]),
            dimensions=(25.0, 25.0))
        self.__region.shown = True
        self.register(pf.EVENT_UPDATE_START, Barracks.__on_update, weakref.ref(self))
        self.register(pf.EVENT_ENTITY_DEATH, Barracks.__on_death, weakref.ref(self))
        super(Barracks, self).__init__(path, pfobj, name, 
            max_hp = 400,
            base_dmg = 0,
            base_armour = 0.6,
            required_resources = Barracks.required_resources(),
            **kwargs)

    def __del__(self):
        self.unregister(pf.EVENT_UPDATE_START, Barracks.__on_update)
        self.unregister(pf.EVENT_ENTITY_DEATH, Barracks.__on_death)
        super(Barracks, self).__del__()

    def __on_update(self, event):
        self.__region.position=(self.pos[0] + self.bounds[0]/2.0 + 15, self.pos[2])

    def __on_death(self, event):
        self.__region.shown = False

    def action(self, idx):

        if not self.completed:
            return super(Barracks, self).action(idx)

        if idx == (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS) - (k.ACTION_NUM_COLS):
            return action.ActionDesc(
                name = "Train Warrior",
                icon_normal="assets/icons/portraits/warrior-normal.jpg",
                icon_hover ="assets/icons/portraits/warrior-hover.jpg",
                icon_active="assets/icons/portraits/warrior-active.jpg",
                func = ut.UnitRecipe(
                    cls       = game.units.slav_warrior.SlavWarrior,
                    building  = self,
                    region    = self.__region,
                    units     = [(game.units.slav_peasant.SlavPeasant, 1)],
                    resources = {"Gold" : 20, "Iron" : 20},
                    population = 0,
                ).train_action,
                ent = self,
                hotkey = pf.SDLK_w,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_RECIPE,
                    ([(game.units.slav_peasant.SlavPeasant, 1)], {"Gold" : 20, "Iron" : 20},
                    "A standard melee fighting unit, the backbone of the Slavs' army.")))
        if idx == (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS) - (k.ACTION_NUM_COLS) + 1:
            return action.ActionDesc(
                name = "Train Elite Warrior",
                icon_normal="assets/icons/portraits/elite-warrior-normal.png",
                icon_hover ="assets/icons/portraits/elite-warrior-hover.png",
                icon_active="assets/icons/portraits/elite-warrior-active.png",
                func = ut.UnitRecipe(
                    cls       = game.units.elite_slav_warrior.EliteSlavWarrior,
                    building  = self,
                    region    = self.__region,
                    units     = [(game.units.slav_warrior.SlavWarrior, 1)],
                    resources = {"Gold" : 20, "Iron" : 20},
                    population = 0,
                    validate_unit_hook = lambda u, cls: not isinstance(u, game.units.elite_slav_warrior.EliteSlavWarrior)
                ).train_action,
                ent = self,
                hotkey = pf.SDLK_e,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_RECIPE,
                    ([(game.units.slav_warrior.SlavWarrior, 1)], {"Gold" : 20, "Iron" : 20},
                    "A better trained and more powerful Warrior.")))
        if idx == (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS) - (k.ACTION_NUM_COLS) + 2:
            return action.ActionDesc(
                name = "Train Yaga Walker",
                icon_normal="assets/icons/portraits/yaga-walker-normal.png",
                icon_hover ="assets/icons/portraits/yaga-walker-hover.png",
                icon_active="assets/icons/portraits/yaga-walker-active.png",
                func = ut.UnitRecipe(
                    cls       = game.units.yaga_walker.YagaWalker,
                    building  = self,
                    region    = self.__region,
                    units     = [(game.units.chicken.Chicken, 1)],
                    resources = {"Wood" : 50},
                    population = 1,
                ).train_action,
                ent = self,
                hotkey = pf.SDLK_y,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_RECIPE,
                    ([(game.units.chicken.Chicken, 1)], {"Wood" : 50},
                    "A sturdy and hard-hitting ranged unit.")))
        return super(Barracks, self).action(idx)

    @staticmethod
    def required_resources():
        return {"Wood" : 200}

    def pr_portrait(self):
        return "assets/images/portraits/barracks.jpg"

    def pr_icon(self):
        return "assets/images/portraits/barracks.jpg"

    def pr_name(self):
        return "Barracks"

    def pr_description(self):
        return "Hopefully gets the recruits ready enough to survive their first battle, where they will really learn."

