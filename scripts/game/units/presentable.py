#
#  This file is part of EVERGLORY
#  Copyright (C) 2021 Eduard Permyakov 
#  All rights reserved.
#

import pf
import weakref
from abc import ABCMeta, abstractproperty

class Presentable(pf.Entity):
    """ 
    Mixin base that extends entities with some properties needed to properly
    display them in the game UI.
        
    """
    __metaclass__ = ABCMeta

    @abstractproperty
    def pr_portrait(self): 
        """ Path of the image file used for large portait in the selection view """
        pass

    @abstractproperty
    def pr_icon(self): 
        """ Path of the image file used for small portait in the selection view """
        pass

    @abstractproperty
    def pr_name(self): 
        """ The string for the unit type name as shown in the UI """
        pass

    @abstractproperty
    def pr_description(self): 
        """ A short description of the unit """
        pass

