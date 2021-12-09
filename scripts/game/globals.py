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

scene_objs = []
scene_regions = []

# Networking #
localIP     = "127.0.0.1"
localPort   = 4567
bufferSize  = 1024

joined = None # False if hosting, True if joined a game, None if singleplayer
UDPServerSocket = None
address = None
port = None

# Helper functions #
import pickle
import re
SEPARATOR = b'Xx\nyy\nxX' # Hopefully unique..
def send(b, entID=None, data=None):
    b = bytes(b)
    # Add our identification if any
    if entID is not None:
        b += SEPARATOR + bytes(entID)
    if data is not None:
        b += SEPARATOR + pickle.dumps(data) #.decode() # .decode() converts string to bytes
    print("send'ing:",bytes_,entID,data,"to",(address, port))
    UDPServerSocket.sendto(b, (address, port))
    print("sent")
def recv():
    bytes_ = None
    entID = None
    data = None
    while True:
        print("recv'ing")
        (bytes_, addressAndPort) = UDPServerSocket.recvfrom(bufferSize)
        # Ignore other addresses and ports
        add, po = addressAndPort
        if add != address or po != port:
            if add != address:
                print("incorrect addrs:",add,address)
            if po != port:
                print("incorrect ports:",po,port)
            continue
        # Get entID and data from the bytes_, if any
        fields = bytes_.split(SEPARATOR) #re.split(SEPARATOR, bytes_)
        bytes_ = fields[0]
        if len(fields) > 1:
            entID = fields[1]
        if len(fields) > 2:
            data = pickle.loads(fields[2]) #.decode("utf-8")) # Bytes to string to unpickled object of some type
        break
    print("recv'ed:",bytes_,entID,data)
    return bytes_, entID, data
# #
# #

# Lib #
import inspect
# Turns something like AnimMoveable.__move_action into its name
# https://stackoverflow.com/questions/961048/get-class-that-defined-method
# def get_class_that_defined_method(meth):
#     for cls in inspect.getmro(meth.im_class):
#         if meth.__name__ in cls.__dict__: 
#             return cls
#     return None

# Custom based on the above:
def get_class_that_defined_method(meth):
    c = meth.im_self if meth.im_self is not None else meth.im_class
    for cls in inspect.getmro(c):
        if meth.__name__ in cls.__dict__ or "_" + c.__name__ + meth.__name__ in cls.__dict__: # Sometimes there are names like `_AnimMoveable__on_motion_begin`
            return cls
    return None


# https://stackoverflow.com/questions/961048/get-class-that-defined-method/961057#961057
# def get_class_that_defined_method(method):
#     method_name = method.__name__
#     if method.__self__:    
#         classes = [method.__self__.__class__]
#     else:
#         #unbound method
#         classes = [method.im_class]
#     while classes:
#         c = classes.pop()
#         if method_name in c.__dict__:
#             return c
#         else:
#             classes = list(c.__bases__) + classes
#     return None

# #
