#!/usr/bin/python
# Pythoni Tk demo interface to mochad. Pressing GUI buttons sends commands to
# mochad to turn lights on,off,bright,dim,xdim,all_lights_on,all_units_off.
# No error checking or fault recovery.
#
# The slider and bright/dim buttons send extended dim commands so will only
# work with lamp modules that support extended dim. All recent (3-4 years)
# LM465 lamp modules with soft-start also support extended dim.
#
# Shows status of a DS10A door/window sensor. The button background color is
# RED when the door is open(alert). The background color changes to GREEN when
# the the door is closed(normal).
#
# Should work on other supported Python platforms such as Windows and MacOS.
# Tested on Ubuntu Linux 10.04 and 10.10. Tested on on Windows XP SP3 after
# installing ActivePython Community Edition. See the following for details.
# http://www.activestate.com/activepython
#
# On Ubuntu/Debian systems you may have to install python-tk like this.
# $ sudo apt-get install python-tk
#

from Tkinter import * 
import socket 
import re

# Change MOCHADHOST to "localhost" if this program runs on the same host as 
# mochad. Otherwise, change to the correct IP.
MOCHADHOST="192.168.1.254"
MOCHADPORT=1099
# Change default house code
HOUSECODE="B"

class App:

    def __init__(self, master):

        frame = Frame(master)
        frame.grid()

        self._job = None
# Paint row like this: Living(B1) On Off Bright Dim <slider>
        Label(frame, text="Living(B1)").grid(row=0)
        self.unit1_on = Button(frame, text="On", command=self.unit1_on_cb)
        self.unit1_on.grid(row=0,column=1)

        self.unit1_off = Button(frame, text="Off", command=self.unit1_off_cb)
        self.unit1_off.grid(row=0,column=2)

        self.unit1_bright = Button(frame, text="Bright", command=self.unit1_bright_cb)
        self.unit1_bright.grid(row=0,column=3)

        self.unit1_dim = Button(frame, text="Dim", command=self.unit1_dim_cb)
        self.unit1_dim.grid(row=0,column=4)

        self.unit1_slider = Scale(frame, from_=0, to=63, orient=HORIZONTAL,
                showvalue=0, command=self.unit1_slider_cb);
        self.unit1_slider.grid(row=0,column=5)

# Paint row like this: Garage(B2) On Off Bright Dim <slider>
        Label(frame, text="Garage(B2)").grid(row=1)
        self.unit2_on = Button(frame, text="On", command=self.unit2_on_cb)
        self.unit2_on.grid(row=1,column=1)

        self.unit2_off = Button(frame, text="Off", command=self.unit2_off_cb)
        self.unit2_off.grid(row=1,column=2)

        self.unit2_bright = Button(frame, text="Bright", command=self.unit2_bright_cb)
        self.unit2_bright.grid(row=1,column=3)

        self.unit2_dim = Button(frame, text="Dim", command=self.unit2_dim_cb)
        self.unit2_dim.grid(row=1,column=4)

        self.unit2_slider = Scale(frame, from_=0, to=63, orient=HORIZONTAL,
                showvalue=0, command=self.unit2_slider_cb);
        self.unit2_slider.grid(row=1,column=5)

        Label(frame, text="Bed 1(B3)").grid(row=2)
        self.unit3_on = Button(frame, text="On", command=self.unit3_on_cb)
        self.unit3_on.grid(row=2,column=1)

        self.unit3_off = Button(frame, text="Off", command=self.unit3_off_cb)
        self.unit3_off.grid(row=2,column=2)

        self.unit3_bright = Button(frame, text="Bright", command=self.unit3_bright_cb)
        self.unit3_bright.grid(row=2,column=3)

        self.unit3_dim = Button(frame, text="Dim", command=self.unit3_dim_cb)
        self.unit3_dim.grid(row=2,column=4)

        self.unit3_slider = Scale(frame, from_=0, to=63, orient=HORIZONTAL,
                showvalue=0, command=self.unit3_slider_cb);
        self.unit3_slider.grid(row=2,column=5)

        Label(frame, text="Bed 2(B4)").grid(row=3)
        self.unit4_on = Button(frame, text="On", command=self.unit4_on_cb)
        self.unit4_on.grid(row=3,column=1)

        self.unit4_off = Button(frame, text="Off", command=self.unit4_off_cb)
        self.unit4_off.grid(row=3,column=2)

        self.unit4_bright = Button(frame, text="Bright", command=self.unit4_bright_cb)
        self.unit4_bright.grid(row=3,column=3)

        self.unit4_dim = Button(frame, text="Dim", command=self.unit4_dim_cb)
        self.unit4_dim.grid(row=3,column=4)

        self.unit4_slider = Scale(frame, from_=0, to=63, orient=HORIZONTAL,
                showvalue=0, command=self.unit4_slider_cb);
        self.unit4_slider.grid(row=3,column=5)

        Label(frame, text="All").grid(row=4)
        self.unitall_on = Button(frame, text="Lamps On", command=self.unitall_on_cb)
        self.unitall_on.grid(row=4,column=1)

        self.unitall_off = Button(frame, text="Units Off", command=self.unitall_off_cb)
        self.unitall_off.grid(row=4,column=2)

        Label(frame, text="Sensor").grid(row=5)
        self.frontdoor = Button(frame, text="Front Door", bg='#00FF00')
        self.frontdoor.grid(row=5,column=1)

# Callback functions invoked when buttons pressed or slider moved.
# Change "pl" to "rf" if using a CM19A.
    def unit1_on_cb(self):
        print "unit 1 on"
        self.unit1_slider.set(63)
        mochad("pl " + HOUSECODE + "1 on\n")

    def unit1_off_cb(self):
        print "unit 1 off"
        self.unit1_slider.set(0)
        mochad("pl " + HOUSECODE + "1 off\n")

    def unit1_bright_cb(self):
        print "unit 1 bright"
        self.unit1_slider.set(self.unit1_slider.get() + 1)
        #mochad("pl " + HOUSECODE + "1 bright\n")

    def unit1_dim_cb(self):
        print "unit 1 dim"
        self.unit1_slider.set(self.unit1_slider.get() - 1)
        #mochad("pl " + HOUSECODE + "1 dim\n")

    def unit1_slider_cb(self,value):
        print "unit1_slider_cb"
        if self._job:
            root.after_cancel(self._job)
        self._job = root.after(200, self.unit1_slider_doit)

    def unit1_slider_doit(self):
        print "unit1_slider_doit"
        mochad("pl " + HOUSECODE + "1 xdim " + str(self.unit1_slider.get()) + "\n")

    def unit2_on_cb(self):
        print "unit 2 on"
        self.unit2_slider.set(63)
        mochad("pl " + HOUSECODE + "2 on\n")

    def unit2_off_cb(self):
        print "unit 2 off"
        self.unit2_slider.set(0)
        mochad("pl " + HOUSECODE + "2 off\n")

    def unit2_bright_cb(self):
        print "unit 2 bright"
        self.unit2_slider.set(self.unit2_slider.get() + 1)
        #mochad("pl " + HOUSECODE + "2 bright\n")

    def unit2_dim_cb(self):
        print "unit 2 dim"
        self.unit2_slider.set(self.unit2_slider.get() - 1)
        #mochad("pl " + HOUSECODE + "2 dim\n")

    def unit2_slider_cb(self,value):
        print "unit2_slider_cb"
        if self._job:
            root.after_cancel(self._job)
        self._job = root.after(200, self.unit2_slider_doit)

    def unit2_slider_doit(self):
        print "unit2_slider_doit"
        mochad("pl " + HOUSECODE + "2 xdim " + str(self.unit2_slider.get()) + "\n")

    def unit3_on_cb(self):
        print "unit 3 on"
        self.unit3_slider.set(63)
        mochad("pl " + HOUSECODE + "3 on\n")

    def unit3_off_cb(self):
        print "unit 3 off"
        self.unit3_slider.set(0)
        mochad("pl " + HOUSECODE + "3 off\n")

    def unit3_bright_cb(self):
        print "unit 3 bright"
        self.unit3_slider.set(self.unit3_slider.get() + 1)
        #mochad("pl " + HOUSECODE + "3 bright\n")

    def unit3_dim_cb(self):
        print "unit 3 dim"
        self.unit3_slider.set(self.unit3_slider.get() - 1)
        #mochad("pl " + HOUSECODE + "3 dim\n")

    def unit3_slider_cb(self,value):
        print "unit3_slider_cb"
        if self._job:
            root.after_cancel(self._job)
        self._job = root.after(200, self.unit3_slider_doit)

    def unit3_slider_doit(self):
        print "unit3_slider_doit"
        mochad("pl " + HOUSECODE + "3 xdim " + str(self.unit3_slider.get()) + "\n")

    def unit4_on_cb(self):
        print "unit 4 on"
        self.unit4_slider.set(63)
        mochad("pl " + HOUSECODE + "4 on\n")

    def unit4_off_cb(self):
        print "unit 4 off"
        self.unit4_slider.set(0)
        mochad("pl " + HOUSECODE + "4 off\n")

    def unit4_bright_cb(self):
        print "unit 4 bright"
        self.unit4_slider.set(self.unit4_slider.get() + 1)
        #mochad("pl " + HOUSECODE + "4 bright\n")

    def unit4_dim_cb(self):
        print "unit 4 dim"
        self.unit4_slider.set(self.unit4_slider.get() - 1)
        #mochad("pl " + HOUSECODE + "4 dim\n")

    def unit4_slider_cb(self,value):
        print "unit4_slider_cb"
        if self._job:
            root.after_cancel(self._job)
        self._job = root.after(200, self.unit4_slider_doit)

    def unit4_slider_doit(self):
        print "unit4_slider_doit"
        mochad("pl " + HOUSECODE + "4 xdim " + str(self.unit4_slider.get()) + "\n")

    def unitall_on_cb(self):
        print "unit all on"
        self.unit1_slider.set(63)
        self.unit2_slider.set(63)
        self.unit3_slider.set(63)
        self.unit4_slider.set(63)
        mochad("pl " + HOUSECODE + " all_lights_on\n")

    def unitall_off_cb(self):
        print "unit all off"
        self.unit1_slider.set(0)
        self.unit2_slider.set(0)
        self.unit3_slider.set(0)
        self.unit4_slider.set(0)
        mochad("pl " + HOUSECODE + " all_units_off\n")

# Send command to mochad
def mochad(command):
    Sock.sendall(command)

# Callback to read from socket
def getmochad(so, mask):
    aLine = so.makefile().readline().rstrip('\r\n')
    print aLine
    if bed1eventnorm.match(aLine):
        app.frontdoor.config(bg='#00FF00')
        print 'Front Door Closed'
    elif bed1eventalert.match(aLine):
        app.frontdoor.config(bg='#FF0000')
        print 'Front Door Open'

# Compile regular expressions to match a specific DS10A module. To add support
# for another DS10A, clone these two lines then change the address.
bed1eventnorm = re.compile('.*Rx RFSEC Addr: AF:74:00 Func: .*_normal_.*_DS10A')
bed1eventalert = re.compile('.*Rx RFSEC Addr: AF:74:00 Func: .*_alert_.*_DS10A')

root = Tk()
app = App(root)

# Open socket to mochad hostname:port
Sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
Sock.connect((MOCHADHOST, MOCHADPORT))

# Tell Tk to call the function getmochad when data is available on Sock
# This is the magic spell to get sensor updates while Tk is running.
root.createfilehandler(Sock, READABLE, getmochad)

root.mainloop()
