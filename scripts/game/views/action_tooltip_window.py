#
#  This file is part of EVERGLORY
#  Copyright (C) 2020 Eduard Permyakov 
#  All rights reserved.
#

import pf
from game.action import ActionTooltipBodyDesc as ATBD
import game.units.presentable as pr
#import common.views.slav_embroidery_style_window as sesw

class ActionTooltipWindow(pf.Window): #sesw.SlavEmbroideryStyleWindow):

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

    
    YELLOW = (180, 110, 20, 255)
    GREEN = (35, 110, 0, 255)

    def __init__(self, x, y, width, height, name, hotkey, tooltip_desc,
        resize_mask = pf.ANCHOR_X_RIGHT | pf.ANCHOR_Y_BOT):
        vresx, vresy = (1920, 1080)
        super(ActionTooltipWindow, self).__init__("ActionTooltip." + str(id(self)), (x, y, width, height),
            pf.NK_WINDOW_BORDER | pf.NK_WINDOW_NO_SCROLLBAR, (vresx, vresy),
            resize_mask = resize_mask,
            suspend_on_pause = True)
        self.name = name
        self.hotkey = hotkey
        self.tooltip_desc = tooltip_desc

    def __tooltip_text(self, text):
        self.layout_row_dynamic(200, 1)
        self.label_colored_wrap(text, self.ACCENT_CLR[:3])

    def __tooltip_recipe(self, units, resources, desc, cooldown=0):

        if cooldown > 0:
            self.layout_row_dynamic(15, 1)
            self.label_colored("Cooldown: %d" % (cooldown,), pf.NK_TEXT_ALIGN_LEFT | pf.NK_TEXT_ALIGN_MIDDLE, 
                self.GREEN[0:3])

        if (len(units) + len(resources.items())) > 0:
            self.layout_row_dynamic(15, 1)
            self.label_colored("Required:", pf.NK_TEXT_ALIGN_LEFT | pf.NK_TEXT_ALIGN_MIDDLE, self.YELLOW[0:3])

        for i in range(len(units) + len(resources.items())):
            if i % 2 == 0:
                self.layout_row_dynamic(15, 2)
            if i < len(units):
                cls, count = units[i]
                assert issubclass(cls, pr.Presentable)
                self.label_colored("    %s (%d)" % (cls.pr_name.im_func(None), count), 
                    pf.NK_TEXT_ALIGN_LEFT | pf.NK_TEXT_ALIGN_MIDDLE, self.YELLOW[0:3])
            else:
                rname, ramount = resources.items()[i - len(units)]
                self.label_colored("    %s (%d)" % (rname, ramount), 
                    pf.NK_TEXT_ALIGN_LEFT | pf.NK_TEXT_ALIGN_MIDDLE, self.YELLOW[0:3])

        nlines = self.text_lines(desc)
        self.layout_row_dynamic(20 * nlines, 1)
        self.label_colored_wrap(desc,self.ACCENT_CLR[0:3])

    #@sesw.SlavEmbroideryStyleWindow.style_decorator
    def update(self):

        label = self.name
        if self.hotkey is not None:
            label += " (" + pf.get_key_name(self.hotkey) + ")"

        self.layout_row_dynamic(15, 1)
        self.image("assets/images/decor-border.png")
        self.layout_row_begin(pf.NK_DYNAMIC, 15, 3)
        self.layout_row_push(0.07)
        self.image("assets/images/decor-symbol.png")
        self.layout_row_push(0.86)
        self.label_colored(label, pf.NK_TEXT_CENTERED, self.ACCENT_CLR[:3])
        self.layout_row_push(0.07)
        self.image("assets/images/decor-symbol.png")
        self.layout_row_dynamic(15, 1)
        self.image("assets/images/decor-border.png")

        if self.tooltip_desc.type == ATBD.TOOLTIP_TEXT:
            self.__tooltip_text(*self.tooltip_desc.args)
        elif self.tooltip_desc.type == ATBD.TOOLTIP_RECIPE:
            self.__tooltip_recipe(*self.tooltip_desc.args)

