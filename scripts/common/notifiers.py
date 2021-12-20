#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref

class Notifier(pf.Task):
    def __new__(cls, *args, **kwargs):
        return super(Notifier, cls).__new__(cls, small_stack=True)
    def __init__(self, master, event):
        self.master = weakref.ref(master)
        self.event = event
    def __run__(self):
        while True:
            val = self.await_event(self.event)
            if self.master() is None or self.master().completed:
                break
            self.send(self.master(), (self.event, val))

class QueuedNotifier(pf.Task):
    """
    Like the basic Notifier, but is guaranteed never to miss an event
    """

    def __on_event(self, user, event):
        self.deque.append(event)

    def __new__(cls, *args, **kwargs):
        return super(QueuedNotifier, cls).__new__(cls, small_stack=True)

    def __init__(self, master, event):
        self.master = weakref.ref(master)
        self.event = event
        self.deque = []

    def __run__(self):
        pf.register_event_handler(self.event, self.__on_event, None)
        done = False
        while not done:
            _ = self.await_event(self.event)
            while len(self.deque) > 0:
                if self.master() is None or self.master().completed:
                    done = True
                    break
                val = self.deque.pop(0)
                self.send(self.master(), (self.event, val))
        pf.unregister_event_handler(self.event, self.__on_event)

