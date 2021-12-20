#
#  This file is part of EVERGLORY
#  Copyright (C) 2020-2021 Eduard Permyakov 
#  All rights reserved.
#

import common.timer
import units.controllable as cont
import game.globals
import pf

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

    def __init__(self, name, icon_normal, icon_hover, icon_active, func, ent, args=(), kwargs={}, 
            hotkey=None, disable_predicate=lambda: False, icon_disabled=None, 
            tooltip_desc=ActionTooltipBodyDesc.empty()):
        self.name = name
        self.icon_normal = icon_normal
        self.icon_hover = icon_hover
        self.icon_active = icon_active
        self.icon_disabled = icon_normal if icon_disabled is None else icon_disabled
        self.ent = ent
        def func_wrapper():
            # Send this action over the network, then call the function it wraps
            if game.globals.joined is not None: # and isinstance(self.ent, cont.Controllable):
                # An action is performed on a specific unit and at a specific position. So use its ID:
                pos = pf.map_pos_under_cursor() # TODO: if a unit is clicked, don't use this but use the clicked unit's entID?
                c = game.globals.get_class_that_defined_method(func)
                if c is None:
                    # TODO: Wrap it in a new class maybe like `foo = dict(x=1, y=2)` from https://stackoverflow.com/questions/1123000/does-python-have-anonymous-classes
                    print("Class for method",func,"was None")
                else:
                    actionFuncName = c.__name__ + '.' + func.__name__
                    print(self.ent.__dict__)
                    game.globals.send('action' + actionFuncName #self.name # Action type identification
                                      , entID=self.ent.__uid__, data=pos)
            func()
        self.func = func_wrapper # This function is called when you press the button in the toolbar.. I thought it would be when you press the button in the toolbar *and then* click on what you want to use it on..
        self.args = args
        self.kwargs = kwargs
        self.hotkey = hotkey
        self.disable_predicate = disable_predicate
        self.tooltip_desc = tooltip_desc
        self.text = ""

class CooldownActionDesc(ActionDesc):

    def __init__(self, name, icon_normal, icon_hover, icon_active, func, ent, cooldown, args=(), kwargs={}, 
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
                                                 func_wrapper, ent, args, kwargs, hotkey, disable_pred_wrapper, icon_disabled, tooltip_desc)

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

