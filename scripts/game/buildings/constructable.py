#
#  This file is part of EVERGLORY
#  Copyright (C) 2021 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref
from abc import ABCMeta, abstractproperty

import game.constants as k
import game.action
import game.globals


class abstractstatic(staticmethod):
    __slots__ = ()
    __isabstractmethod__ = True
    def __init__(self, function):
        super(abstractstatic, self).__init__(function)
        function.__isabstractmethod__ = True

class Constructable(pf.BuildableEntity):
    """ 
    Mixin base that adds a 'cancel construction' button and a 'required resources' 
    property to buildable types.
        
    """
    __metaclass__ = ABCMeta

    @abstractproperty
    def required_resources():
        """ A dictionary of the resources required to found this building """
        pass

    def __cancel_construction_action(self):
        self.notify(pf.EVENT_ENTITY_DEATH, weakref.ref(self))
        game.globals.scene_objs.remove(self)
        self.zombiefy()

    def action(self, idx):

        if not self.supplied:
            if idx == (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS) - 1:
                return game.action.ActionDesc(
                    name = "Cancel Construction",
                    icon_normal="assets/icons/cancel-command-normal.png",
                    icon_hover ="assets/icons/cancel-command-hover.png",
                    icon_active="assets/icons/cancel-command-active.png",
                    func = self.__cancel_construction_action,
                    hotkey = pf.SDLK_c,
                    tooltip_desc = game.action.ActionTooltipBodyDesc(
                        game.action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                        ("Cancel construction of this building, razing the build site",)))
            else:
                return None
        return super(Constructable, self).action(idx)

