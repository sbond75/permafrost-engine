#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import game.constants as k
import common.view_controllers.view_controller as vc
import game.units.controllable as cont
import weakref

class ActionPadVC(vc.ViewController):

    def __init__(self, view):
        self.__view = view
        self.__hotkey_action_map = {}
        self.__hotkeys_stack = []
        self.__actions_stack = []

    def __install_hotkeys(self, actions):
        for act in actions:
            if act and act.hotkey:
                self.__hotkey_action_map[act.hotkey] = act

    def __on_selection_changed(self, event):
        self.__hotkeys_stack = []
        self.__actions_stack = []
        self.__hotkey_action_map = {}
        self.__view.clear_actions()

        sel = pf.get_unit_selection()
        controllable_sel = [ent for ent in sel if isinstance(ent, cont.Controllable)]

        if len(controllable_sel) > 0:
            first = controllable_sel[0]
            fac_list = pf.get_factions_list()
            if fac_list[first.faction_id]["controllable"]:
                actions = [first.action(i) for i in range(0, k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS)]
                self.__install_hotkeys(actions)
                self.__view.actions = actions

    def __on_update(self, event):
        if len(self.__actions_stack) > 0:
            return
        self.__on_selection_changed(None)

    def __on_keydown(self, event):
        keysym = event[1]
        if keysym in self.__hotkey_action_map and not pf.ui_text_edit_has_focus():
            act = self.__hotkey_action_map[keysym]
            if not act.disable_predicate():
                act.func(*act.args, **act.kwargs)

    def __on_actions_push(self, event):
        self.__hotkeys_stack += [self.__hotkey_action_map]
        self.__actions_stack += [self.__view.actions]
        self.__hotkey_action_map = {}

        self.__install_hotkeys(event)
        self.__view.actions = event

    def __on_actions_pop(self, event):
        try:
            self.__view.actions = self.__actions_stack.pop()
            self.__hotkey_action_map = self.__hotkeys_stack.pop()
        except IndexError:
            pass

    def __on_dialog_show(self, event):
        self.__view.hide()

    def __on_dialog_hide(self, event):
        self.__view.show()

    def activate(self):
        # Don't use 'register_ui_event_handler' as we want the action pad to be disabled/frozen when paused
        pf.register_event_handler(pf.EVENT_UNIT_SELECTION_CHANGED, ActionPadVC.__on_selection_changed, self)
        pf.register_event_handler(pf.EVENT_UPDATE_START, ActionPadVC.__on_update, self)
        pf.register_event_handler(pf.SDL_KEYDOWN, ActionPadVC.__on_keydown, self)
        pf.register_event_handler(k.EVENT_PUSH_ACTIONS, ActionPadVC.__on_actions_push, self)
        pf.register_event_handler(k.EVENT_POP_ACTIONS, ActionPadVC.__on_actions_pop, self)
        pf.register_event_handler(k.EVENT_DIALOG_SHOW, ActionPadVC.__on_dialog_show, self)
        pf.register_event_handler(k.EVENT_DIALOG_HIDE, ActionPadVC.__on_dialog_hide, self)
        self.__view.show()

    def deactivate(self):
        self.__view.hide()
        pf.unregister_event_handler(k.EVENT_POP_ACTIONS, ActionPadVC.__on_actions_pop)
        pf.unregister_event_handler(k.EVENT_PUSH_ACTIONS, ActionPadVC.__on_actions_push)
        pf.unregister_event_handler(pf.SDL_KEYDOWN, ActionPadVC.__on_keydown)
        pf.unregister_event_handler(pf.EVENT_UPDATE_START, ActionPadVC.__on_update)
        pf.unregister_event_handler(pf.EVENT_UNIT_SELECTION_CHANGED, ActionPadVC.__on_selection_changed)
        pf.unregister_event_handler(k.EVENT_DIALOG_SHOW, ActionPadVC.__on_dialog_show)
        pf.unregister_event_handler(k.EVENT_DIALOG_HIDE, ActionPadVC.__on_dialog_hide)

