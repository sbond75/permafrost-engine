import socket
from zeroconf import ServiceInfo, Zeroconf
import netifaces
import time

# Returns a string.
def get_my_ip_address():
    """
    Return the/a network-facing IP number for this system.
    """
    ifs = netifaces.interfaces()
    print(ifs)

    # Try some common ones
    # TODO: run on multiple interfaces (wifi, ethernet, etc.)
    for if_ in ['wlan0', 'wlp2s0']:
        if not if_ in ifs:
            continue
        addrs = netifaces.ifaddresses(if_)[netifaces.AF_INET] # Get ipv4 addresses list (there can be multiple per interface! -- https://pypi.org/project/netifaces/ )
        addr = addrs[0] # We choose the first ip
        ip = addr['addr'] # my ip
        print("get_my_ip_address():", addrs, ip)
        return ip

def advertise(self):
        postfix = self.config['global']['service_prefix']
        self.port = int(self.config['global']['port'])
        #print(self.config['device']['hostname']+postfix)
        info = ServiceInfo(postfix, self.config['device']['hostname']+"."+postfix,
                       socket.inet_aton(self.ip), self.port, 0, 0,
                       {'info': self.config['device']['description']}, "defenders_of_paradise.local.")
        print(info)

        #self.bindConnection()

        zeroconf = Zeroconf()
        zeroconf.register_service(info)

        print("Ready")

        return (zeroconf, info)

def run():
    class Object(object):
        pass

    a = Object()
    a.config = {'global': {'service_prefix': '_http._tcp.local.',
                           'port': '4567'},
                'device': {'hostname': socket.gethostname(),
                           'description': 'descTemp'}}
    a.ip = get_my_ip_address()
    return advertise(a)

def stop(pair):
    zeroconf, info = pair
    print("Unregistering...")
    zeroconf.unregister_service(info)
    zeroconf.close()
