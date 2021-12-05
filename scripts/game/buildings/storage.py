#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref

import common.disappearing_text_task as dtt

import game.action as action
import game.constants as k
import game.units.controllable as cont
import game.views.storage_config_window as scw
import game.view_controllers.storage_config_vc as scvc

class Storage(pf.StorageSiteEntity):
    """ 
    Mixin base for storage site entities with a common configuration widget 
    and set of effects.
    """

    POS_CLR = (22, 84, 5, 255)
    NEG_CLR = (84, 5, 5, 255)

    def __init__(self, path, pfobj, name, **kwargs):
        super(Storage, self).__init__(path, pfobj, name, **kwargs)
        self.__scvc = scvc.StorageConfigVC(scw.StorageConfigWindow(self))
        self.register(pf.EVENT_STORAGE_SITE_AMOUNT_CHANGED, Storage.__on_amount_changed, weakref.ref(self))
        self.register(k.EVENT_UP_STORAGE_DESIRED, Storage.__on_up_desired, weakref.ref(self))
        self.register(k.EVENT_DOWN_STORAGE_DESIRED, Storage.__on_down_desired, weakref.ref(self))

    def __del__(self):
        self.__scvc.deactivate()
        self.unregister(pf.EVENT_STORAGE_SITE_AMOUNT_CHANGED, Storage.__on_amount_changed)
        self.unregister(k.EVENT_UP_STORAGE_DESIRED, Storage.__on_up_desired)
        self.unregister(k.EVENT_DOWN_STORAGE_DESIRED, Storage.__on_down_desired)
        super(Storage, self).__del__()

    def __on_amount_changed(self, event):
        name = event[0]
        amount = event[1]
        text = "%+d %s" % (amount, name)
        color = self.POS_CLR if amount > 0 else self.NEG_CLR
        dtt.FollowingDisappearingTextTask(text, self, (-25, -10, 80, 25), color, 5000, travel=100).run()
        pf.global_event(k.EVENT_STORAGE_AMOUNT_CHANGED, None)

    def __on_up_desired(self, event):
        rname = event[0]
        delta = event[1]
        self.set_desired(rname, self.get_desired(rname) + delta)

    def __on_down_desired(self, event):
        rname = event[0]
        delta = event[1]
        self.set_desired(rname, self.get_desired(rname) - delta)

    def action(self, idx):
        if idx == 0 and self.completed:
            return action.ActionDesc(
                name = "Configure Storage",
                icon_normal="assets/icons/config-storage-command-normal.png",
                icon_hover ="assets/icons/config-storage-command-hover.png",
                icon_active="assets/icons/config-storage-command-active.png",
                func = self.__config_storage_action,
                hotkey = pf.SDLK_c,
                tooltip_desc = action.ActionTooltipBodyDesc(
                    action.ActionTooltipBodyDesc.TOOLTIP_TEXT,
                    ("Bring up a dialog to set the \"desired\" amount for every kind of "
                    "resource that this building is able to store. The \"desired\" amounts "
                    "will mainly determine which resouces and in what quantity will be brought "
                    "to the building with a \"Transport\" order. You can also forbid units from "
                    "taking any resources from this site.",)))
        return super(Storage, self).action(idx)

    def __config_storage_action(self):
        self.__scvc.activate()

