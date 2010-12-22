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

#use strict; 
use Socket; 

my $hostname = `/bin/uname -n`;
chomp($hostname);
my $server = '127.0.0.1';       # www server also is running mochad
my $port   = 1099;
my $hosttype = `/bin/uname -m`;

my $server2 = '192.168.1.24';   # Seagate DockStar running OpenWrt and mochad
my $port2 = 1099;
my $hostname2 = "dstar2";

my $server3 = '192.168.1.26';   # Seagate DockStar running OpenWrt and mochad
my $port3 = 1099;
my $hostname3 = "dstar4";

my $rootdir;
if (index($hosttype, "86") >= 0) {
    $rootdir = '.';              # Chumby or x86 Linux
}
else {
    $rootdir = '/wwwx10/cgi-bin';   # OpenWrt
}
my $chumbywww = 'x10.pl';

require "$rootdir/netcat.pl";
require "$rootdir/getsensors.pl";
require "$rootdir/cgi-lib.pl";

sub paintform 
{
    print <<EndOfForm;
        <form action="$chumbywww" method="post">
            <P>
            <input type="submit"   name="UnitB1" value="On">
            <input type="submit"   name="UnitB1" value="Off">
            <label>Living(B1)</label><br>

            <input type="submit"   name="UnitB2" value="On">
            <input type="submit"   name="UnitB2" value="Off">
            <label>Garage(B2)</label><br>

            <input type="submit"   name="UnitB3" value="On">
            <input type="submit"   name="UnitB3" value="Off">
            <input type="submit"   name="UnitB3" value="Dim">
            <input type="submit"   name="UnitB3" value="Bright">
            <label>Bedroom 1(B3)</label><br>

            <input type="submit"   name="UnitB4" value="On">
            <input type="submit"   name="UnitB4" value="Off">
            <input type="submit"   name="UnitB4" value="Dim">
            <input type="submit"   name="UnitB4" value="Bright">
            <label>Bedroom 2(B4)</label><br>

            <input type="submit"   name="UnitB5" value="On">
            <input type="submit"   name="UnitB5" value="Off">
            <label>Master (B5)</label><br>

            <input type="submit"   name="UnitC1" value="On">
            <input type="submit"   name="UnitC1" value="Off">
            <label>Living TV(C1)</label><br>

            <input type="submit"   name="UnitC2" value="On">
            <input type="submit"   name="UnitC2" value="Off">
            <label>Office fan(C2)</label><br>

            <input type="submit"   name="UnitD1" value="On">
            <input type="submit"   name="UnitD1" value="Off">
            <label>Cable modem(D1)</label><br>

            <input type="submit"   name="UnitD2" value="On">
            <label>Chime(D2)</label><br>

            <input type="submit"   name="Function" value="Lights On">
            <input type="submit"   name="Function" value="Lights Off">
            <input type="submit"   name="Function" value="Units Off">
            House: <select name="House">
            <option>A
            <option selected>B
            <option>C
            <option>D
            <option>E
            <option>F
            <option>G
            <option>H
            <option>I
            <option>J
            <option>K
            <option>L
            <option>M
            <option>N
            <option>O
            <option>P
            </select><br>
            <input type="submit"   name="Function" value="Refresh">
            <input type="submit"   name="Function" value="Arm">
            <input type="submit"   name="Function" value="Disarm">
            </P>
        </form>
EndOfForm
    getsensors($server, 1099, $hostname);
    #getsensors($server2, 1099, $hostname2);
    getsensors($server3, 1099, $hostname3);
}

my ($x10func, $housecode, $unitcode);
my ($varname, $varvalue);

print PrintHeader();
print '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">';
print HtmlTop("Lights");

if (!ReadParse()) {
    paintform();
}
else {
    $x10func = "";
    $housecode = "";
    $unitcode = "";
    foreach $varname (keys (%in)) {
        $varvalue = $in{$varname};
        #print "$varname = $varvalue<br>";
        if ($varname eq "Function") {
            $x10func = $varvalue;
        }
        elsif ($varname eq "House") {
            $housecode = $varvalue;
        }
        elsif ($varname =~ /^Unit/) {
            $x10func = $varvalue;
            $unitcode = $varname;
            $unitcode =~ s/^Unit//;
        }
        else {
            print "Unknown varname $varname<br>";
        }
    }
    if ($housecode ne "") {
        # Change blanks to underlines in function names
        $x10func =~ tr/ /_/;
        #print "pl $housecode$unitcode $x10func<br>\n";
        if ($x10func eq "Arm") {
            netcat($server, $port, "rfsec 0x11 arm\n");
        }
        elsif ($x10func eq "Disarm") {
            netcat($server, $port, "rfsec 0x11 disarm\n");
        }
        elsif ($x10func ne "Refresh") {
            if (length($unitcode) == 2) {
                netcat($server, $port, "pl $unitcode $x10func\n");
                #netcat($server2, $port2, "pl $unitcode $x10func\n");
            }
            else {
                netcat($server, $port, "pl $housecode$unitcode $x10func\n");
                #netcat($server2, $port2, "pl $housecode$unitcode $x10func\n");
            }
        }
    }
    paintform();
}

print HtmlBot();
exit;
