#
#  This file is part of EVERGLORY
#  Copyright (C) 2021 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref
import common.notifiers as notifiers
import game.units.status_holder as sh
import common.disappearing_text_task as dtt

class ReplenishAbility(pf.Task):

    def __init__(self, resource, amount, maximum, period, statusname, statusicon):
        self.resource = weakref.ref(resource)
        self.amount = amount
        self.maximum = maximum
        self.period = period
        self.statusname = statusname
        self.statusicon = statusicon

    def __run__(self):

        notifiers.QueuedNotifier(self, pf.EVENT_1HZ_TICK).run()
        duration_left = self.period

        if isinstance(self.resource(), sh.StatusHolder):
            self.resource().add_status(self.statusname, self.statusicon, self.period)

        while True:
            sender, msg = self.receive()
            event, arg = msg
            self.reply(sender, None)

            if not self.resource() or self.resource().zombie or getattr(self.resource(), "dying", False):
                break

            if event == pf.EVENT_1HZ_TICK:

                duration_left -= 1
                if isinstance(self.resource(), sh.StatusHolder):
                    self.resource().get_status(self.statusname).duration_left = duration_left

                if duration_left == 0:
                    newamount = min(self.resource().resource_amount + self.amount, self.maximum)
                    self.resource().resource_amount = newamount
                    dtt.FollowingDisappearingTextTask("Replenished", self.resource(), (-25, -10, 80, 25), 
                        (45, 175, 55, 255), 5000, travel=100).run()
                    duration_left = self.period
                    if isinstance(self.resource(), sh.StatusHolder):
                        self.resource().get_status(self.statusname).duration_left = duration_left

