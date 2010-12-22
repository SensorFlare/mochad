#!/usr/bin/perl -w
# 
# Copyright 2010 Brian Uechi <buasst@gmail.com>
# 
# This file is part of mochad.
# 
# mochad is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
# 
# mochad is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with mochad.  If not, see <http://www.gnu.org/licenses/>.
# 

# This example applet shows to translate security sensor changes from RF
# security sensors (MS10A and DS10A) to power line house/unit on/off. The
# IO::Socket module is used which makes the code much easier and shorter.

# Round 2: The RF security sensors send updates about every 90 minutes
# which could be annoying. For example, if a door is left intensionally
# left open, the DS10A will send an alert about every 90 minutes. The
# solution is to track the last known state of the sensor then send
# PL commands only when the state changes. 
# Initially, the state of the senso is unknown. After its first transmission,
# the state (alert or normal) is saved. Following transmissions update the
# last known state.
#
# RF security device address from MS10A or DS10A => House/Unit code
# For example, C6:1B:00 could be the address of an MS10A motion sensor and
# M1 could be the address of a chime module. Whenever motion is sensed, the
# chime modules chimes.
#
# EF:43:80 could be the address of a DS10A door/window sensor and M2 could
# be the address of a lamp module. When a garage door is open, the lamp turns on.
# When the door is closed, the lamp turns off. In addition, 
#
# Start Customize by user 
my %RFSEC_Sensors = (
    'EF:43:80' => 'M1',     # DS10A and lamp
    'C6:1B:00' => 'M2',     # MS10A and chime
);

my %RFSEC_Speak = (
    'EF:43:80' => 'Garage door',  # DS10A voice message
);

my %RFSEC_State;

my $Mochadhost = "biceff";      # mochad hostname or IP address
my $Mochadport = "1099";

# End Customize by user 

use IO::Socket;

# Run Festival Lite text-to-speech. flite does all the hard work.
sub speak
{
    system("/usr/bin/flite -t \"'@_'\"");
}

my ($txrx, $plrf, $junk, $secaddr, $x10func, $mochad, $last_state);

$mochad = IO::Socket::INET->new(
                      Proto    => "tcp",
                      PeerAddr => "$Mochadhost",
                      PeerPort => "$Mochadport",
                  )
                or die "cannot connect to mochad at biceff:1099";

#Sample event message from mochad
#12/18 20:19:29 Rx RFSEC Addr: C6:1B:00 Func: Motion_alert_MS10A

while ( $aLine = <$mochad> ) {
    chomp($aLine);
    ($junk, $junk, $txrx, $plrf, $junk, $secaddr, $junk,$x10func) = 
        split(/ /, $aLine);
    if ($txrx eq "Rx" && $plrf eq "RFSEC") {
        if (exists($RFSEC_Sensors{$secaddr})) {
            if (exists($RFSEC_State{$secaddr})) {
                $last_state = $RFSEC_State{$secaddr};
            }
            else {
                $last_state = "unknown";
            }
            if ($x10func =~ /alert/) {
                if ($last_state ne "alert") {
                    # This could be changed to send via RF by changng "pl" to
                    # "rf".  This would be useful if mochad is connected to a
                    # CM19A which does not have a power line interface.
                    #$mochad->print("rf $RFSEC_Sensors{$secaddr} on\n");
                    $mochad->print("pl $RFSEC_Sensors{$secaddr} on\n");
                    if (exists($RFSEC_Speak{$secaddr})) {
                        speak("$RFSEC_Speak{$secaddr} open");
                    }
                    $RFSEC_State{$secaddr} = "alert";
                }
            }
            elsif ($x10func =~ /normal/) {
                if ($last_state ne "normal") {
                    #$mochad->print("rf $RFSEC_Sensors{$secaddr} on\n");
                    $mochad->print("pl $RFSEC_Sensors{$secaddr} off\n");
                    if (exists($RFSEC_Speak{$secaddr})) {
                        speak("$RFSEC_Speak{$secaddr} closed");
                    }
                    $RFSEC_State{$secaddr} = "normal";
                }
            }
        }
    }
}
