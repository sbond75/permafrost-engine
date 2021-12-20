#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref
import game.constants as k
import common.notifiers as notifiers

class BuildingPlacementController(pf.Task):

    def __init__(self, building, workers):
        self.building = building
        self.workers = weakref.WeakSet(workers)
        if pf.map_pos_under_cursor():
            self.building.pos = pf.map_pos_under_cursor()

    def __run__(self):

        notifiers.Notifier(self, pf.SDL_MOUSEMOTION).run()
        notifiers.Notifier(self, pf.SDL_MOUSEBUTTONDOWN).run()
        pf.set_click_move_enabled(False)

        n = notifiers.QueuedNotifier(self, k.EVENT_BUILDING_CHOSEN_FOR_PLACEMENT)
        n.run()

        pf.global_event(k.EVENT_BUILDING_CHOSEN_FOR_PLACEMENT, id(self.building))
        self.yield_()

        while True:
            sender, msg = self.receive()
            event, arg = msg
            self.reply(sender, None)

            if event == pf.SDL_MOUSEMOTION:
                pos = pf.map_pos_under_cursor()
                if pos:
                    self.building.pos = pos
            elif event == k.EVENT_BUILDING_CHOSEN_FOR_PLACEMENT:
                if arg != id(self.building):
                    break
            elif event == pf.SDL_MOUSEBUTTONDOWN:
                if arg[0] == pf.SDL_BUTTON_LEFT and not pf.mouse_over_ui() and not pf.mouse_over_minimap():
                    if len(n.deque) > 0:
                        break
                    if self.building.unobstructed():
                        self.building.mark()
                        for worker in self.workers:
                            worker.notify(k.EVENT_BUILDING_MARKED, self.building)
                        pf.global_event(k.EVENT_POP_ACTIONS, None)
                    else:
                        msg = "Could not place building at (%.0f, %.0f, %.0f)" % self.building.pos
                        pf.global_event(k.EVENT_NOTIFICATION_TEXT, msg)
                pf.set_click_move_enabled(True)
                break

        del self.building

