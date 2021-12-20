#
#  This file is part of EVERGLORY
#  Copyright (C) 2021 Eduard Permyakov 
#  All rights reserved.
#

import pf
import math
import weakref
import common.notifiers as notifiers
import game.units.status_holder as sh
import game.constants as k

class BlessingSpell(pf.Task):

    CAST_RANGE = 65
    DURATION = 10

    def __valid_target(self, unit):
        if not unit:
            return False
        if unit.zombie:
            return False
        if not isinstance(unit, pf.CombatableEntity):
            return False
        if isinstance(unit, pf.BuildableEntity):
            return False
        if getattr(unit, "dying", False):
            return False
        if unit.faction_id != self.caster().faction_id:
            return False
        return True

    def __init__(self, caster):
        self.caster = weakref.ref(caster)

    def __run__(self):

        notifiers.Notifier(self, pf.SDL_MOUSEBUTTONDOWN).run()
        notifiers.Notifier(self, pf.EVENT_UPDATE_START).run()
        notifiers.QueuedNotifier(self, pf.EVENT_1HZ_TICK).run()

        pf.set_click_move_enabled(False)
        pf.set_cursor_rts_mode(False)
        pf.activate_system_cursor(pf.CURSOR_TARGET)
        pf.disable_unit_selection()

        target_unit = None
        range = pf.Region(
            type=pf.REGION_CIRCLE,
            name="Blessing." + str(id(self)),
            position=(self.caster().pos[0], self.caster().pos[2]),
            radius=self.CAST_RANGE)
        range.shown = True
        spell_active = False
        duration_left = self.DURATION
        max_hp = 0
        bird = None

        while True:
            sender, msg = self.receive()
            event, arg = msg
            self.reply(sender, None)

            if not self.caster() or self.caster().zombie or getattr(self.caster(), "dying", False):
                break

            if event == pf.SDL_MOUSEBUTTONDOWN:

                if spell_active:
                    continue

                if arg[0] == pf.SDL_BUTTON_RIGHT or pf.mouse_over_ui() or pf.mouse_over_minimap():
                    break

                target_unit = pf.get_hovered_unit()
                if not self.__valid_target(target_unit):
                    pf.global_event(k.EVENT_NOTIFICATION_TEXT, "Spell must target a friendly combat unit.")
                    pf.play_global_effect("error")
                    break

                dx = self.caster().pos[0] - target_unit.pos[0]
                dz = self.caster().pos[2] - target_unit.pos[2]
                dist = math.sqrt(math.pow(dx, 2) + math.pow(dz, 2))
                if dist > self.CAST_RANGE:
                    pf.global_event(k.EVENT_NOTIFICATION_TEXT, "Spell target is outside the casting range.")
                    pf.play_global_effect("error")
                    break

                target_unit.ping()
                self.caster().notify(k.EVENT_SPELL_CAST, "Blessing")
                if isinstance(target_unit, sh.StatusHolder):
                    target_unit.add_status("Blagostrazh's Blessing", "assets/icons/blessing-icon-normal.png", duration_left)

                pf.activate_system_cursor(pf.CURSOR_POINTER)
                pf.set_cursor_rts_mode(True)
                pf.set_click_move_enabled(True)
                pf.enable_unit_selection()

                range.shown = False
                spell_active = True
                max_hp = target_unit.max_hp
                target_unit.max_hp = 0

                bird = pf.AnimEntity("assets/models/bird", "bird.pfobj", "Bird", idle_clip="Hover")
                bird.scale = (5.0, 5.0, 5.0)
                bird.rotation = target_unit.rotation
                bird.pos = (target_unit.pos[0], target_unit.pos[1] + target_unit.height + 2.5, target_unit.pos[2])

            elif event == pf.EVENT_UPDATE_START:
                if not spell_active:
                    range.position = (self.caster().pos[0], self.caster().pos[2])
                else:
                    bird.pos = (target_unit.pos[0], target_unit.pos[1] + target_unit.height + 2.5, target_unit.pos[2])
                    bird.rotation = target_unit.rotation

            elif event == pf.EVENT_1HZ_TICK:

                if not spell_active:
                    continue

                duration_left -= 1
                if isinstance(target_unit, sh.StatusHolder):
                    target_unit.get_status("Blagostrazh's Blessing").duration_left = duration_left

                if duration_left == 0:
                    if isinstance(target_unit, sh.StatusHolder):
                        target_unit.remove_status("Blagostrazh's Blessing")
                    break

        if not spell_active:
            pf.activate_system_cursor(pf.CURSOR_POINTER)
            pf.set_cursor_rts_mode(True)
            pf.set_click_move_enabled(True)
            pf.enable_unit_selection()
        else:
            target_unit.max_hp = max_hp
            target_unit.hp = target_unit.max_hp
            bird.zombiefy()

