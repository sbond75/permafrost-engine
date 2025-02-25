#
#  This file is part of Permafrost Engine. 
#  Copyright (C) 2018-2020 Eduard Permyakov 
#
#  Permafrost Engine is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Permafrost Engine is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
#  Linking this software statically or dynamically with other modules is making 
#  a combined work based on this software. Thus, the terms and conditions of 
#  the GNU General Public License cover the whole combination. 
#  
#  As a special exception, the copyright holders of Permafrost Engine give 
#  you permission to link Permafrost Engine with independent modules to produce 
#  an executable, regardless of the license terms of these independent 
#  modules, and to copy and distribute the resulting executable under 
#  terms of your choice, provided that you also meet, for each linked 
#  independent module, the terms and conditions of the license of that 
#  module. An independent module is a module which is not derived from 
#  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
#  extend this exception to your version of Permafrost Engine, but you are not 
#  obliged to do so. If you do not wish to do so, delete this exception 
#  statement from your version.
#

############################################################
# Shared constants                                         #
############################################################

ACTION_NUM_ROWS = 3
ACTION_NUM_COLS = 4

############################################################
# Module-specific events                                   #
############################################################

EVENT_CONTROLLED_FACTION_CHANGED = 0x20000
EVENT_HOST                       = 0x200001
EVENT_HOST_ACCEPTED              = 0x2000011
EVENT_JOIN                       = 0x200002
EVENT_JOIN_ACCEPTED              = 0x2000021
EVENT_SETTINGS_SHOW              = 0x20001
EVENT_PERF_SHOW                  = 0x20002
EVENT_SIMSTATE_CHANGE            = 0x20003
EVENT_SESSION_SHOW               = 0x20004
EVENT_BUILDING_CHOSEN_FOR_PLACEMENT = 0x20016
EVENT_SPELL_CAST                    = 0x20029

EVENT_PUSH_ACTIONS                  = 0x20005
EVENT_POP_ACTIONS                   = 0x20006

EVENT_DIALOG_SHOW                   = 0x2002b
EVENT_DIALOG_HIDE                   = 0x2002c
