#
#  This file is part of EVERGLORY
#  Copyright (C) 2020-2021 Eduard Permyakov 
#  All rights reserved.
#

import common.timer

# Unique token ot be returned from an action func 
# to know we shouldn't start the cooldown 
ACTION_FAIL_TOKEN = id(False)

class ActionTooltipBodyDesc(object):

    TOOLTIP_EMPTY = 0x0
    TOOLTIP_TEXT = 0x1
    TOOLTIP_RECIPE = 0x2

    def __init__(self, type, args):
        self.type = type
        self.args = args

    @classmethod
    def empty(cls):
        return ActionTooltipBodyDesc(cls.TOOLTIP_EMPTY, ())

class ActionDesc(object):

    def __init__(self, name, icon_normal, icon_hover, icon_active, func, args=(), kwargs={}, 
            hotkey=None, disable_predicate=lambda: False, icon_disabled=None, 
            tooltip_desc=ActionTooltipBodyDesc.empty()):
        self.name = name
        self.icon_normal = icon_normal
        self.icon_hover = icon_hover
        self.icon_active = icon_active
        self.icon_disabled = icon_normal if icon_disabled is None else icon_disabled
        self.func = func
        self.args = args
        self.kwargs = kwargs
        self.hotkey = hotkey
        self.disable_predicate = disable_predicate
        self.tooltip_desc = tooltip_desc
        self.text = ""

class CooldownActionDesc(ActionDesc):

    def __init__(self, name, icon_normal, icon_hover, icon_active, func, cooldown, args=(), kwargs={}, 
            hotkey=None, disable_predicate=lambda: False, icon_disabled=None,
            tooltip_desc=ActionTooltipBodyDesc.empty()):

        self.cd_duration = cooldown
        self.cd_elapsed = 0
        self.cd_active = False

        def disable_pred_wrapper():
            if self.cd_active:
                return True
            return disable_predicate()

        def func_wrapper(*args, **kwargs):
            status = func(*args, **kwargs)
            if status is not ACTION_FAIL_TOKEN:
                self.start_cooldown()

        super(CooldownActionDesc, self).__init__(name, icon_normal, icon_hover, icon_active, 
            func_wrapper, args, kwargs, hotkey, disable_pred_wrapper, icon_disabled, tooltip_desc)

    def start_cooldown(self):
        def on_tick(arg):
            self.cd_elapsed += 1
            self.text = str(self.cd_duration - self.cd_elapsed)
        def on_finish(arg):
            self.cd_active = False
            self.text = ""
        self.cd_active = True
        self.cd_elapsed = 0
        self.text = str(self.cd_duration - self.cd_elapsed)
        common.timer.Timer(
            nsecs = self.cd_duration,
            on_tick = on_tick,
            on_finish = on_finish,
            arg = None,
        ).run()

