#
#  This file is part of EVERGLORY
#  Copyright (C) 2021 Eduard Permyakov 
#  All rights reserved.
#

import pf

class Status(object):

    def __init__(self, name, icon, duration_left=None):
        self.name = name
        self.icon = icon
        self.duration_left = duration_left

class StatusHolder(pf.Entity):
    """
    Mixin base to help keep track of the currently applied statuses (upgrades, ailments, etc.)
    to the current entity.
    """

    def __init__(self, *args, **kwargs):
        self.statuses = []
        super(StatusHolder, self).__init__(*args, **kwargs)

    def add_status(self, name, icon, duration_left=None):
        self.statuses.append(Status(name, icon, duration_left))

    def remove_status(self, name):
        found = next((s for s in self.statuses if s.name == name), None)
        if found:
            self.statuses.remove(found)

    def has_status(self, name):
        found = next((s for s in self.statuses if s.name == name), None)
        return found is not None

    def get_status(self, name):
        return next((s for s in self.statuses if s.name == name), None)

