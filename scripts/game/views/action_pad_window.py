#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
import game.constants as k
import game.action
import game.views.action_tooltip_window as atw

import common.button_style_ctx as btc
#import common.views.slav_embroidery_style_window as sesw

class ActionPadWindow(pf.Window): #sesw.SlavEmbroideryStyleWindow):

    LIGHT_HIGHLIGHT_CLR = (238, 220, 194, 255)
    DARK_HIGHLIGHT_CLR = (226, 201, 165, 255)
    WHITE = (255, 255, 255, 255)
    BLACK = (0, 0, 0, 255)
    LIGHT_GRAY = (200, 200, 200, 255)
    BUTTON_CLR = (238, 238, 218, 255)
    LABEL_CLR = (0, 0, 0, 255) 
    ACCENT_CLR = (198, 37, 45, 255)
    LIGHT_ACCENT_CLR = (215, 84, 90, 255)
    DARK_ACCENT_CLR = (150, 59, 63, 255)
    CLEAR = (0, 0, 0, 0)

    
    BUTTON_WIDTH = 75
    BUTTON_PADDING = 6
    DISABLED_BG_COLOR = (0, 0, 0, 0)
    DISABLED_TEXT_COLOR = (60, 60, 60, 255)
    TOOLTIP_HEIGHT = 200

    def __init__(self):
        # Each button also has a 1px border around it, hence the (ACTION_NUM_COLS*2)
        width = ActionPadWindow.BUTTON_WIDTH * k.ACTION_NUM_COLS \
            + (k.ACTION_NUM_COLS-1) * ActionPadWindow.BUTTON_PADDING +(k.ACTION_NUM_COLS*2) + 4
        height = ActionPadWindow.BUTTON_WIDTH * k.ACTION_NUM_ROWS \
            + (k.ACTION_NUM_ROWS-1) * ActionPadWindow.BUTTON_PADDING +(k.ACTION_NUM_ROWS*2) + 7
        vresx, vresy = (1920, 1080)
        super(ActionPadWindow, self).__init__("ActionPad", (vresx - width - 10, vresy - height - 10, 
            width, height), pf.NK_WINDOW_BORDER | pf.NK_WINDOW_NO_SCROLLBAR, (vresx, vresy),
            resize_mask = pf.ANCHOR_X_RIGHT | pf.ANCHOR_Y_BOT,
            suspend_on_pause = True)
        self.spacing = (float(ActionPadWindow.BUTTON_PADDING), float(ActionPadWindow.BUTTON_PADDING))
        self.padding = (2.0, 4.0)
        self.clear_actions()
        self.tooltip_window = None
        self.hover_idx = -1

    def disabled_button_style(self):
        return {
            "normal" : ActionPadWindow.DISABLED_BG_COLOR, 
            "hover" : ActionPadWindow.DISABLED_BG_COLOR,
            "active" : ActionPadWindow.DISABLED_BG_COLOR,
            "text_normal" : ActionPadWindow.DISABLED_TEXT_COLOR, 
            "text_hover" : ActionPadWindow.DISABLED_TEXT_COLOR,
            "text_active" : ActionPadWindow.DISABLED_TEXT_COLOR,
        }

    def __image_button(self, img_normal, img_hover, img_active, action, args, text):
        with btc.ButtonStyle(normal=img_normal, hover=img_hover, active=img_active,
                             text_normal=self.ACCENT_CLR, text_active=self.ACCENT_CLR, text_hover=self.ACCENT_CLR):
            def wrapped(*args):
                pf.play_global_effect("button-press", interrupt=True)
                action(*args)
            return pf.Window.button_label(self, text, wrapped, args)

    def __image_button_disabled(self, img, text):
        with btc.ButtonStyle(normal=img, hover=img, active=img,
                             text_normal=self.ACCENT_CLR, text_active=self.ACCENT_CLR, text_hover=self.ACCENT_CLR):
            return pf.Window.button_label(self, text, lambda: None)

    def clear_actions(self):
        self.actions = [None] * (k.ACTION_NUM_ROWS * k.ACTION_NUM_COLS)

    #@sesw.SlavEmbroideryStyleWindow.style_decorator
    def update(self):
        with btc.ButtonStyle(padding=(0.0, 0.0), rounding=0.0):

            any_hovered = False
            for r in range(0, k.ACTION_NUM_ROWS):
                self.layout_row_static(ActionPadWindow.BUTTON_WIDTH, ActionPadWindow.BUTTON_WIDTH, k.ACTION_NUM_COLS)

                for c in range(0, k.ACTION_NUM_COLS):
                    action = self.actions[r * k.ACTION_NUM_COLS + c]
                    if action:
                        if action.disable_predicate():
                            button_hovered = self.__image_button_disabled(action.icon_disabled, action.text)
                        else:
                            button_hovered = self.__image_button(
                                img_normal = action.icon_normal, 
                                img_hover = action.icon_hover, 
                                img_active = action.icon_active,
                                action = action.func,
                                args = action.args,
                                text = action.text)
                        if button_hovered:
                            any_hovered = True
                            idx = r * k.ACTION_NUM_COLS + c
                            if self.hover_idx != idx:
                                tooltip_bounds = (
                                    self.position[0], 
                                    self.position[1] - self.TOOLTIP_HEIGHT - 10,
                                    self.size[0],
                                    self.TOOLTIP_HEIGHT)
                                self.tooltip_window = atw.ActionTooltipWindow(
                                    *tooltip_bounds, 
                                    name = action.name, 
                                    hotkey = action.hotkey, 
                                    tooltip_desc = action.tooltip_desc)
                                self.tooltip_window.show()
                                self.hover_idx = idx
                    else:
                        self.button_label("", lambda: None) #self.disabled_button_label("", lambda: None)

            if not any_hovered:
                self.tooltip_window = None
                self.hover_idx = -1

