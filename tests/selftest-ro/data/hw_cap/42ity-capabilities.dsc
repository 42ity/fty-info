hardware
    type              = ipc
    gpi
        base_address  = 488     #   Target address of the GPIO chipset (gpiochip488 on IPC3000)
        count         = 10      #   Number of GPI (on IPC3000)
        offset        = -1      #   GPI pins have -1 offset, i.e. GPI 1 is pin 0, ... (on IPC3000)
#        mapping                #   Mapping between GPI number and HW pin number
#   <gpi number> = <pin number>
    gpo
        base_address  = 488     #   Target address of the GPIO chipset (gpiochip488 on IPC3000)
        count         =  5      #   Number of GPO (on IPC3000)
        offset        = 20      #   GPO pins have +20 offset, i.e. GPO 1 is pin 21, ... (on IPC3000)
        mapping                 #   Mapping between GPO number and HW pin number
        #   <gpo number> = <pin number>
            p4 = 502
            p5 = 503
