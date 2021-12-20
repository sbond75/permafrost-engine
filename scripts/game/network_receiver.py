#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref
import game.constants as k
import common.notifiers as notifiers
import game.globals
import re
import socket

class NetworkReceiver(pf.Task):

    def __init__(self):
        pass

    def __on_move_issued(self, event):
        print("__on_move_issued",event)
        
    def __on_order_issued(self, event):
        print("__on_order_issued",event)
    
    def __run__(self):
        pf.register_event_handler(pf.EVENT_MOVE_ISSUED, NetworkReceiver.__on_move_issued, self) # Basic move action that the engine produces if you right-click without pressing an action
        pf.register_event_handler(pf.EVENT_ORDER_ISSUED, NetworkReceiver.__on_order_issued, self)
        # TODO: unregister_event_handler the above

        #notifiers.Notifier(self, pf.SDL_MOUSEMOTION).run()
        #notifiers.Notifier(self, pf.SDL_MOUSEBUTTONDOWN).run()
        #pf.set_click_move_enabled(False)

        #n = notifiers.QueuedNotifier(self, k.EVENT_BUILDING_CHOSEN_FOR_PLACEMENT)
        #n.run()

        #pf.global_event(k.EVENT_BUILDING_CHOSEN_FOR_PLACEMENT, id(self.building))
        #self.yield_()

        # TODO: HACK to fix: don't wait on I/O.. poll instead, with yield.. #
        game.globals.UDPServerSocket.settimeout(100 # milliseconds (will be converted to seconds below:)
                                                / 1000.0
                                                )
        # #
        while True:
            # Receive from network
            try:
                b, entID, data = game.globals.recv()
            except socket.timeout as e:
                #print("Handled timeout exception:",e)
                self.yield_()
                continue
            if b.startswith(b'action'):
                # Process action
                actionName = re.search(b'action(.*)', b).group(1)
                actionPos = data
                # For now, simply set unit selection. We should back up the current unit selection with pf.get_unit_selection(), then pf.set_unit_selection([unit for entID]) then run the action with a new event EVENT_MOVE_ISSUED or something similar but instead make it take a parameter of the position on the map to issue an "action" to -- within the C code, run the correct implementation of the action.
                # Need S_Entity_ObjForUID to be exposed to python. then also get uid for an entity and use that for entID.
                # Need an event for mouse clicks on the map: simply use some set thing like pf.set_attack_on_left_click() (no arguments) and then run this event for mouse clicks.

                print("Action:",actionName,actionPos)
                prevSelection = pf.get_unit_selection()
                
                # Set unit selection
                pf.set_unit_selection([entID])

                # Prepare command
                # TODO: temp hack: #
                if actionName.endswith("__move_action"):
                    # Move command
                    pf.set_move_on_left_click()
                # #

                # Simulate click on map
                pf.perform_simulated_click(actionPos)
                
                # Restore backup
                pf.set_unit_selection(prevSelection)
            else:
                print("Unhandled packet:",b,entID,data)
            
            self.yield_()
            
            
        #     sender, msg = self.receive()
        #     event, arg = msg
        #     self.reply(sender, None)

        #     if event == pf.SDL_MOUSEMOTION:
        #         pos = pf.map_pos_under_cursor()
        #         if pos:
        #             self.building.pos = pos
        #     elif event == k.EVENT_BUILDING_CHOSEN_FOR_PLACEMENT:
        #         if arg != id(self.building):
        #             break
        #     elif event == pf.SDL_MOUSEBUTTONDOWN:
        #         if arg[0] == pf.SDL_BUTTON_LEFT and not pf.mouse_over_ui() and not pf.mouse_over_minimap():
        #             if len(n.deque) > 0:
        #                 break
        #             if self.building.unobstructed():
        #                 self.building.mark()
        #                 for worker in self.workers:
        #                     worker.notify(k.EVENT_BUILDING_MARKED, self.building)
        #                 pf.global_event(k.EVENT_POP_ACTIONS, None)
        #             else:
        #                 msg = "Could not place building at (%.0f, %.0f, %.0f)" % self.building.pos
        #                 pf.global_event(k.EVENT_NOTIFICATION_TEXT, msg)
        #         pf.set_click_move_enabled(True)
        #         break

        # del self.building

