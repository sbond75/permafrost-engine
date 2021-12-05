#
#  This file is part of EVERGLORY
#  Copyright (C) 2021 Eduard Permyakov 
#  All rights reserved.
#

import pf
import common.notifiers as notifiers


class Timer(pf.Task):

    def __new__(cls, *args, **kwargs):
        return super(Timer, cls).__new__(cls, small_stack=True)

    def __init__(self, nsecs, on_tick, on_finish, arg):
        self.nsecs = nsecs
        self.on_tick = on_tick
        self.on_finish = on_finish
        self.arg = arg

    def __run__(self):

        notifiers.QueuedNotifier(self, pf.EVENT_1HZ_TICK).run()
        elapsed = 0

        while elapsed < self.nsecs:

            sender, msg = self.receive()
            event, arg = msg
            self.reply(sender, None)

            if event == pf.EVENT_1HZ_TICK:
                if self.on_tick:
                    self.on_tick(arg)
                elapsed += 1

        if self.on_finish:
            self.on_finish(arg)

