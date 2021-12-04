from zeroconf import ServiceBrowser, Zeroconf


def run(onAddService):
    class MyListener:

        def remove_service(self, zeroconf, type, name):
            print("Service %s removed" % (name,))

        def add_service(self, zeroconf, type, name):
            info = zeroconf.get_service_info(type, name)
            print("Service %s added, service info: %s" % (name, info))
            if onAddService(info):
                zeroconf.close()


    zeroconf = Zeroconf()
    listener = MyListener()
    browser = ServiceBrowser(zeroconf, "_http._tcp.local.", listener)
    # try:
    #     input("Press enter to exit...\n\n")
    # finally:
    #     zeroconf.close()

    #r = zeroconf
    #print(r.get_service_info("_http._tcp.local.", "dop._http._tcp.local."))
