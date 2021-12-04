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

import pf
from constants import *

import common.view_controllers.view_controller as vc
import common.view_controllers.tab_bar_vc as tbvc
import common.view_controllers.video_settings_vc as vsvc
import common.view_controllers.game_settings_vc as gsvc

import common.views.settings_tabbed_window as stw
import common.views.video_settings_window as vsw
import common.views.game_settings_window as gsw
import common.views.perf_stats_window as psw

import common.views.session_window as sw

import common.constants

import common.advertise as ad
from threading import Thread
import socket
import common.zeroconfTesting as sb # service browser

localIP     = "127.0.0.1"
localPort   = 4567
bufferSize  = 1024

class HostView(pf.Window):
    def __init__(self):
        super(HostView, self).__init__("Host Game", (1920 / 2, 1080 / 2, 400, 250), pf.NK_WINDOW_BORDER | pf.NK_WINDOW_MOVABLE | pf.NK_WINDOW_MINIMIZABLE | pf.NK_WINDOW_TITLE |  pf.NK_WINDOW_NO_SCROLLBAR, (1920, 1080))
        pair = ad.run() # Start advertizing our service

        # Wait for UDP packet
        def threaded_function(args):
            print("UDP server starting")
            # Create a datagram socket
            UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
            # Bind to address and ip
            UDPServerSocket.bind((localIP, localPort))
            print("UDP server up and listening")
            (bytes_, address) = UDPServerSocket.recvfrom(bufferSize) # "The return value is a pair (bytes, address) where bytes is a bytes object representing the data received and address is the address of the socket sending the data."
            print("Got message:", bytes_, "from", address)
            # if bytes_ valid, stop the ad:
            if bytes_.startswith('connect'):
                ad.stop(pair)

                clientMsg = "Message from Client:{}".format(bytes_)
                clientIP  = "Client IP Address:{}".format(address)
                
                print(clientMsg)
                print(clientIP)

                # Sending a reply to client
                UDPServerSocket.sendto(bytes('accept', 'utf-8'), address)
        self.thread = Thread(target = threaded_function, args = (1,))
        self.thread.daemon = True # daemon won't stop main thread from finishing
        self.thread.start()
    
    def update(self):
        self.layout_row_dynamic(20, 1)
        self.label_colored_wrap("Waiting for players to join...", (255, 255, 255))


class JoinView(pf.Window):
    def __init__(self):
        super(JoinView, self).__init__("Join Game", (1920 / 2, 1080 / 2, 400, 250), pf.NK_WINDOW_BORDER | pf.NK_WINDOW_MOVABLE | pf.NK_WINDOW_MINIMIZABLE | pf.NK_WINDOW_TITLE |  pf.NK_WINDOW_NO_SCROLLBAR, (1920, 1080))
        self.possible = []

        # Scan for Bonjour? services
        def onAddService(info):
            if info.server.startswith('defenders_of_paradise'):
                # Add to list
                def joinFn():
                    address = socket.inet_ntoa(info.address)
                    print("Joining", info.name, "with address", address, "and port", info.port)
                    # Send packet with UDP
                    # Create a datagram socket
                    UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
                    # Bind to address and ip
                    #UDPServerSocket.bind((address, info.port))
                    print("UDP client up")
                    # Send
                    # TODO: handle corruption or dropped packets like TCP does (this applies to all sockets in these python scripts)..
                    #UDPServerSocket.send('connect'.encode())
                    UDPServerSocket.sendto('connect', (address, info.port))
                self.possible.append((info, joinFn))
                return True
            return False
        sb.run(onAddService)
        
    def update(self):
        self.layout_row_dynamic(20, 1)
        self.label_colored_wrap("Scanning for players on your network...", (255, 255, 255))

        for info, joinFn in self.possible:
            self.button_label(info.name, joinFn)
        
class HostVC(vc.ViewController):
    def __init__(self):
        self.__view = HostView()

    def activate(self):
        self.__view.show()
        
    def deactivate(self):
        self.__view.hide()

class JoinVC(vc.ViewController):
    def __init__(self):
        self.__view = JoinView()
        
    def activate(self):
        self.__view.show()
        
    def deactivate(self):
        self.__view.hide()
    

class IngameVC(vc.ViewController):

    def __init__(self, view):
        self.__view = view
        self.__perf_view = psw.PerfStatsWindow()
        self.__host_vc = HostVC()
        self.__join_vc = JoinVC()
        self.__settings_vc = tbvc.TabBarVC(stw.SettingsTabbedWindow(),
            tab_change_event=common.constants.EVENT_SETTINGS_TAB_SEL_CHANGED)
        self.__settings_vc.push_child("Video", vsvc.VideoSettingsVC(vsw.VideoSettingsWindow()))
        self.__settings_vc.push_child("Game", gsvc.GameSettingsVC(gsw.GameSettingsWindow()))
        self.__host_shown = False
        self.__join_shown = False
        self.__settings_shown = False
        self.__session_view = sw.SessionWindow()

        self.__view.fac_names = [fac["name"] for fac in pf.get_factions_list()]
        assert len(self.__view.fac_names) > 0
        if len(self.__view.fac_names) >= 2:
            self.__view.active_fac_idx = 1
        else:
            self.__view.active_fac_idx = 0

    def __on_controlled_faction_chagned(self, event):
        pf.clear_unit_selection()
        for i in range(len(pf.get_factions_list())):
            pf.set_faction_controllable(i, False) 
        pf.set_faction_controllable(event, True)

    def __on_host(self, event):
        if not self.__host_shown:
            self.__host_vc.activate()
            self.__host_shown = True

    def __on_join(self, event):
        if not self.__join_shown:
            self.__join_vc.activate()
            self.__join_shown = True        
        
    def __on_settings_show(self, event):
        if not self.__settings_shown:
            self.__settings_vc.activate()
            self.__settings_shown = True

    def __on_settings_hide(self, event):
        if self.__settings_shown:
            self.__settings_vc.deactivate()
            self.__settings_shown = False

    def __on_perf_show(self, event):
        if self.__perf_view.hidden:
            self.__perf_view.show()

    def __on_ss_change(self, event):
        pf.set_simstate(event)

    def __on_session_show(self, event):
        if self.__session_view.hidden:
            self.__session_view.show()

    def __on_session_save(self, event):
        self.__session_view.hide()
        ss = pf.get_simstate()
        pf.set_simstate(pf.G_PAUSED_UI_RUNNING)
        print(event)
        print(type(event))
        print(id(event))
        print(hex(id(event)))
        pf.save_session(event)
        pf.set_simstate(ss)

    def __on_session_load(self, event):
        self.__session_view.hide()
        pf.load_session(event)

    def activate(self):
        pf.register_ui_event_handler(EVENT_CONTROLLED_FACTION_CHANGED, IngameVC.__on_controlled_faction_chagned, self)
        pf.register_ui_event_handler(EVENT_HOST, IngameVC.__on_host, self)
        pf.register_ui_event_handler(EVENT_JOIN, IngameVC.__on_join, self)
        pf.register_ui_event_handler(EVENT_SETTINGS_SHOW, IngameVC.__on_settings_show, self)
        pf.register_ui_event_handler(EVENT_PERF_SHOW, IngameVC.__on_perf_show, self)
        pf.register_ui_event_handler(common.constants.EVENT_SETTINGS_HIDE, IngameVC.__on_settings_hide, self)
        pf.register_ui_event_handler(EVENT_SIMSTATE_CHANGE, IngameVC.__on_ss_change, self)
        pf.register_ui_event_handler(EVENT_SESSION_SHOW, IngameVC.__on_session_show, self)
        pf.register_ui_event_handler(common.constants.EVENT_SESSION_SAVE_REQUESTED, IngameVC.__on_session_save, self)
        pf.register_ui_event_handler(common.constants.EVENT_SESSION_LOAD_REQUESTED, IngameVC.__on_session_load, self)
        self.__view.show()

    def deactivate(self):
        self.__view.hide()
        pf.unregister_event_handler(common.constants.EVENT_SESSION_SAVE_REQUESTED, IngameVC.__on_session_save)
        pf.unregister_event_handler(common.constants.EVENT_SESSION_LOAD_REQUESTED, IngameVC.__on_session_load)
        pf.unregister_event_handler(EVENT_SESSION_SHOW, IngameVC.__on_session_show)
        pf.unregister_event_handler(EVENT_SIMSTATE_CHANGE, IngameVC.__on_ss_change)
        pf.unregister_event_handler(common.constants.EVENT_SETTINGS_HIDE, IngameVC.__on_settings_hide)
        pf.unregister_event_handler(EVENT_PERF_SHOW, IngameVC.__on_perf_show, self)
        pf.unregister_event_handler(EVENT_SETTINGS_SHOW, IngameVC.__on_settings_show)
        pf.unregister_event_handler(EVENT_CONTROLLED_FACTION_CHANGED, IngameVC.__on_controlled_faction_chagned)

