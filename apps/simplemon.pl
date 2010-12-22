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

# Simple program to get event messages from mochad.

my $Mochadhost = "biceff";
my $Mochadport = "1099";

use IO::Socket;

my ($txrx, $plrf, $junk, $x10addr, $x10func, $mochad);

$mochad = IO::Socket::INET->new(
                      Proto    => "tcp",
                      PeerAddr => "$Mochadhost",
                      PeerPort => "$Mochadport",
                  )
                or die "cannot connect to mochad at biceff:1099";

#Sample input line from mochad
#12/18 20:19:29 Rx RFSEC Addr: C6:1B:00 Func: Motion_alert_MS10A

while ( $aLine = <$mochad> ) {
    chomp($aLine);
    ($junk, $junk, $txrx, $plrf, $junk, $x10addr, $junk, $x10func) = 
        split(/ /,$aLine);
    print "$txrx $plrf $x10addr $x10func\n";
}
