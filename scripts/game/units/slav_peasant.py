#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref

import game.units.anim_moveable as am
import game.units.presentable as pr
import game.units.transporter as tr
import game.units.status_holder as sh

import game.action
import game.globals
import game.constants as k
import game.building_placement_controller as bpc

import game.buildings.izba
import game.buildings.barracks
import game.buildings.storage_site
import game.buildings.farm
import game.buildings.town_center
import game.buildings.bear_stable
import game.buildings.windmill
import game.buildings.tower
import game.buildings.atti_site


def unit_exists(cls, faction_id):
    for o in game.globals.scene_objs:
        if o.zombie or getattr(o, "dying", False):
            continue
        if isinstance(o, pf.BuildableEntity) and not o.completed:
            continue
        if o.faction_id == faction_id and isinstance(o, cls):
            return True
    return False


class IdleState(object):
    """
    The idle state that the peasant goes through. 'IDLE' and 'ACTIVE' are the
    two core states and 'WAKING' and 'STOPPING' are transient states. The point
    is that the peasant has to be not doing anything for 2 ticks in a row to 
    change states to 'IDLE' and vice versa.
    """
    IDLE = 0
    WAKING = 1
    ACTIVE = 2
    STOPPING = 3

class SlavPeasant(pf.BuilderEntity, tr.Transporter, pf.CombatableEntity, am.AnimMoveable, pr.Presentable, sh.StatusHolder):

    def __new__(cls, 
            path="assets/models/slav_peasant", 
            pfobj="slav-peasant.pfobj", 
            name="SlavPeasant", **kwargs):
        return super(SlavPeasant, cls).__new__(cls, path, pfobj, name, **kwargs)

    def __init__(self, 
            path="assets/models/slav_peasant", 
            pfobj="slav-peasant.pfobj", 
            name="SlavPeasant", **kwargs):
        super(SlavPeasant, self).__init__(path, pfobj, name, 
            idle_clip = ["Idle", "Idle2", "Idle3"][pf.rand(3)],
            build_speed = 10,
            max_hp = 100,
            base_dmg = 0,
            base_armour = 0.3,
            **kwargs)

        self.speed = 20.0
        self.scale = (1.5, 1.5, 1.5)
        self.selectable = True
        self.selection_radius = 3.00
        self.vision_range = 35.0

        self.building = False
        self.harvesting = False
        self.__build_target = None
        self.__harvest_target = None

        self.set_max_carry("Wood", 5)
        self.set_max_carry("Food", 5)
        self.set_max_carry("Iron", 5)
        self.set_max_carry("Gold", 5)

        self.set_gather_speed("Wood", 0.5)
        self.set_gather_speed("Food", 0.5)
        self.set_gather_speed("Iron", 0.5)
        self.set_gather_speed("Gold", 0.5)

        self.idlestate = IdleState.IDLE
        self.dying = False
        self.selection_priority = 5

        self.register(pf.EVENT_BUILD_BEGIN, SlavPeasant.__on_build_begin, weakref.ref(self))
        self.register(pf.EVENT_BUILD_END, SlavPeasant.__clear_building, weakref.ref(self))
        self.register(pf.EVENT_BUILD_FAIL_FOUND, SlavPeasant.__on_fail_found, weakref.ref(self))
        self.register(pf.EVENT_BUILD_TARGET_ACQUIRED, SlavPeasant.__on_build_target_acquired, weakref.ref(self))
        self.register(pf.EVENT_HARVEST_BEGIN, SlavPeasant.__on_harvest_begin, weakref.ref(self))
        self.register(pf.EVENT_HARVEST_END, SlavPeasant.__clear_harvesting, weakref.ref(self))
        self.register(pf.EVENT_HARVEST_TARGET_ACQUIRED, SlavPeasant.__on_harvest_target_acquired, weakref.ref(self))
        self.register(pf.EVENT_MOVE_ISSUED, SlavPeasant.__reset, weakref.ref(self))
        self.register(pf.EVENT_ENTITY_STOP, SlavPeasant.__reset, weakref.ref(self))
        self.register(pf.EVENT_RESOURCE_DROPPED_OFF, SlavPeasant.__reset, weakref.ref(self))
        self.register(pf.EVENT_RESOURCE_PICKED_UP, SlavPeasant.__reset, weakref.ref(self))
        self.register(pf.EVENT_ENTITY_DEATH, SlavPeasant.__on_death, weakref.ref(self))
        self.register(k.EVENT_BUILDING_MARKED, SlavPeasant.__on_building_marked, weakref.ref(self))
        self.register(pf.EVENT_ANIM_CYCLE_FINISHED, SlavPeasant.__on_anim_finished, weakref.ref(self))
        self.register(pf.EVENT_UPDATE_START, SlavPeasant.__on_update, weakref.ref(self))

    def __del__(self):
        self.unregister(pf.EVENT_BUILD_BEGIN, SlavPeasant.__on_build_begin)
        self.unregister(pf.EVENT_BUILD_END, SlavPeasant.__clear_building)
        self.unregister(pf.EVENT_BUILD_FAIL_FOUND, SlavPeasant.__on_fail_found)
        self.unregister(pf.EVENT_BUILD_TARGET_ACQUIRED, SlavPeasant.__on_build_target_acquired)
        self.unregister(pf.EVENT_HARVEST_BEGIN, SlavPeasant.__on_harvest_begin)
        self.unregister(pf.EVENT_HARVEST_END, SlavPeasant.__clear_harvesting)
        self.unregister(pf.EVENT_HARVEST_TARGET_ACQUIRED, SlavPeasant.__on_harvest_target_acquired)
        self.unregister(pf.EVENT_MOVE_ISSUED, SlavPeasant.__reset)
        self.unregister(pf.EVENT_ENTITY_STOP, SlavPeasant.__reset)
        self.unregister(pf.EVENT_RESOURCE_DROPPED_OFF, SlavPeasant.__reset)
        self.unregister(pf.EVENT_RESOURCE_PICKED_UP, SlavPeasant.__reset)
        self.unregister(pf.EVENT_ENTITY_DEATH, SlavPeasant.__on_death)
        self.unregister(k.EVENT_BUILDING_MARKED, SlavPeasant.__on_building_marked)
        self.unregister(pf.EVENT_ANIM_CYCLE_FINISHED, SlavPeasant.__on_anim_finished)
        self.unregister(pf.EVENT_UPDATE_START, SlavPeasant.__on_update)
        super(SlavPeasant, self).__del__()

    def __on_update(self, event):

        prev = self.idlestate
        active = self.moving or self.building or self.harvesting

        if prev == IdleState.IDLE:
            self.idlestate = IdleState.WAKING if active else IdleState.IDLE
        elif prev == IdleState.WAKING:
            self.idlestate = IdleState.ACTIVE if active else IdleState.IDLE
        elif prev == IdleState.ACTIVE:
            self.idlestate = IdleState.STOPPING if not active else IdleState.ACTIVE
        elif prev == IdleState.STOPPING:
            self.idlestate = IdleState.IDLE if not active else IdleState.ACTIVE

        if self.idlestate != prev:
            if self.idlestate == IdleState.ACTIVE:
                pf.global_event(k.EVENT_WORKER_BECAME_NOT_IDLE, self)
            elif self.idlestate == IdleState.IDLE:
                pf.global_event(k.EVENT_WORKER_BECAME_IDLE, self)

    def idle(self):
        return self.idlestate != IdleState.ACTIVE

    def idle_anim(self):
        idle_anims = ["Idle2", "Idle", "Idle2", "Idle3", "Idle2"]
        return idle_anims[pf.rand(len(idle_anims))]

    def __set_idle_model(self):
        if self.total_carry > 0:
            self.set_model("assets/models/slav_peasant", "slav-peasant-carry.pfobj")
        else:
            self.set_model("assets/models/slav_peasant", "slav-peasant.pfobj")
        self.play_anim(self.move_anim() if self.moving else self.idle_anim())

    def move_anim(self):
        if self.total_carry > 0:
            return "Carry"
        else:
            return "Walking"

    def action(self, idx):
        if idx == (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS) - k.ACTION_NUM_COLS:
            return game.action.ActionDesc(
                name = "Gather Resources",
                icon_normal="assets/icons/gather-command-normal.png",
                icon_hover ="assets/icons/gather-command-hover.png",
                icon_active="assets/icons/gather-command-active.png",
                func = self.__gather_action,
                hotkey = pf.SDLK_g,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Order all selected harvester units to gather the target resource. Upon "
                    "gathering the maximum amount they can carry, the harvesters will drop it off "
                    "at the nearest storage site. You may also simply right-click the resource to "
                    "give this order.",)))
        if idx == (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS) - k.ACTION_NUM_COLS + 1:
            return game.action.ActionDesc(
                name = "Build",
                icon_normal="assets/icons/build-command-normal.png",
                icon_hover ="assets/icons/build-command-hover.png",
                icon_active="assets/icons/build-command-active.png",
                func = self.__build_action,
                hotkey = pf.SDLK_b,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Bring up the list of buildings which can be built. Once selected, a bulding "
                    "can be placed on the map with a left click. The first worker unit to approach "
                    "the build site will \"found\" the building. It must then be supplied with the "
                    "required resources before construction can begin.",)))
        if idx == (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS) - k.ACTION_NUM_COLS + 2:
            return game.action.ActionDesc(
                name = "Repair",
                icon_normal="assets/icons/repair-command-normal.png",
                icon_hover ="assets/icons/repair-command-hover.png",
                icon_active="assets/icons/repair-command-active.png",
                func = self.__repair_action,
                hotkey = pf.SDLK_r,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Order all selected worker units to repair a building that is either damaged "
                    "or has not finished construction. You may also simply right-click a damaged or "
                    "incomplete building to give this order.",)))
        return super(SlavPeasant, self).action(idx)

    def __clear_building(self, event=None):
        self.building = False
        self.__build_target = None
        self.__set_idle_model()

    def __clear_harvesting(self, event=None):
        self.harvesting = False
        self.__harvest_target = None
        self.__set_idle_model()

    def __reset(self, event=None):
        self.__clear_building()
        self.__clear_harvesting()

    def __on_building_marked(self, building):
        self.__reset()
        self.__build_target = building
        self.build(building)

    def __on_fail_found(self, event):
        msg = "Could not found building at (%.0f, %.0f, %.0f)" % self.__build_target.pos
        pf.global_event(k.EVENT_NOTIFICATION_TEXT, msg)
        self.__clear_building()

    def __on_build_target_acquired(self, event):
        self.__build_target = event

    def __on_build_begin(self, event):
        if not self.__build_target or self.__build_target.zombie:
            self.__build_target = None
            return
        assert not self.building
        self.building = True
        self.face_towards(self.__build_target.pos)
        self.set_model("assets/models/slav_peasant", "slav-peasant-build.pfobj")
        self.__build_target = None

    def __place_building_action(self, cls, *args, **kwargs):
        building = cls(*args, **kwargs)
        building.faction_id = self.faction_id
        workers = [u for u in pf.get_unit_selection() if isinstance(u, SlavPeasant)]
        bpc.BuildingPlacementController(building, workers).run()

    def __on_harvest_target_acquired(self, event):
        self.__harvest_target = event

    def __on_harvest_begin(self, event):
        if not self.__harvest_target or self.__harvest_target.zombie:
            self.__harvest_target = None
            return
        assert not self.harvesting
        self.harvesting = True
        self.face_towards(self.__harvest_target.pos)
        rname = self.__harvest_target.resource_name
        if rname == "Wood":
            self.set_model("assets/models/slav_peasant", "slav-peasant-chop.pfobj")
        elif rname == "Food" and isinstance(self.__harvest_target, pf.BuildableEntity):
            self.set_model("assets/models/slav_peasant", "slav-peasant-plow.pfobj")
        elif rname == "Food":
            self.set_model("assets/models/slav_peasant", "slav-peasant-forage.pfobj")
        else:
            self.set_model("assets/models/slav_peasant", "slav-peasant-mine.pfobj")

    def __on_anim_finished(self, event):

        if self.harvesting and (self.__harvest_target and not self.__harvest_target.zombie):
            rname = self.__harvest_target.resource_name
            if rname == "Wood":
                pf.play_effect("axe-chop", self.__harvest_target.pos)
            elif rname == "Iron" or rname == "Gold":
                pf.play_effect("pickaxe-hit", self.__harvest_target.pos)

        if self.building:
            pf.play_effect("hammer-strike-01", self.pos)

    def __on_up_prio(self, event):
        self.increase_transport_priority(event)

    def __on_down_prio(self, event):
        self.decrease_transport_priority(event)

    def __on_death_anim_finish(self, event):
        self.unregister(pf.EVENT_ANIM_CYCLE_FINISHED, SlavPeasant.__on_death_anim_finish)
        game.globals.scene_objs.remove(self)

    def __on_death(self, event):
        self.dying = True
        self.clear_curr_carry()
        self.set_model("assets/models/slav_peasant", "slav-peasant.pfobj")
        self.play_anim("Death", mode=pf.ANIM_MODE_ONCE)
        self.register(pf.EVENT_ANIM_CYCLE_FINISHED, SlavPeasant.__on_death_anim_finish, self)

    def __cancel_action(self):
        pf.global_event(k.EVENT_POP_ACTIONS, None)

    def __build_action(self):
        actions = [None] * k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS
        actions[5] = game.action.ActionDesc(
                name = "Barracks",
                icon_normal="assets/icons/portraits/barracks-normal.jpg",
                icon_hover ="assets/icons/portraits/barracks-hover.jpg",
                icon_active="assets/icons/portraits/barracks-active.jpg",
                func = self.__place_building_action,
                ent = self,
                args = (game.buildings.barracks.Barracks,),
                hotkey = pf.SDLK_b,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_RECIPE,
                    ([], game.buildings.barracks.Barracks.required_resources(),
                    "A basic military structure which allows training and upgrading warriors.")))
        actions[7] = game.action.ActionDesc(
                name = "Tower",
                icon_normal="assets/icons/portraits/tower-normal.jpg",
                icon_hover ="assets/icons/portraits/tower-hover.jpg",
                icon_active="assets/icons/portraits/tower-active.jpg",
                func = self.__place_building_action,
                ent = self,
                args = (game.buildings.tower.Tower,),
                hotkey = pf.SDLK_d,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_RECIPE,
                    ([], game.buildings.tower.Tower.required_resources(),
                    "A defensive structure which fires arrows at any enemy units in its' range.")))
        actions[-1] = game.action.ActionDesc( 
                name = "Cancel",
                icon_normal="assets/icons/cancel-command-normal.png",
                icon_hover ="assets/icons/cancel-command-hover.png",
                icon_active="assets/icons/cancel-command-active.png",
                func = self.__cancel_action,
                ent = self,
                hotkey = pf.SDLK_c,
                tooltip_desc = game.action.ActionTooltipBodyDesc(
                    game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Quit the build menu, going back to the actions menu.",)))
        pf.global_event(k.EVENT_PUSH_ACTIONS, actions)

    def __repair_action(self):
        pf.set_build_on_left_click()

    def __gather_action(self):
        pf.set_gather_on_left_click()

    def pr_portrait(self):
        return "assets/images/portraits/kholop.jpg"

    def pr_icon(self):
        return "assets/images/portraits/kholop.jpg"

    def pr_name(self):
        return "Kholop"

    def pr_description(self):
        return "A simple man, but capable of great things."

    def voicelines(self):
        return ["voicelines/kholop/%02d" % (i+1,) for i in range(0, 10)]

