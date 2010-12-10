#!/usr/bin/perl -Tw

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

# Dump output from 3 mochads. Useful for debugging and logging.
# The first character on the line indicates which mochad the rest of the line
# came from. The mochads are specified by IP address and port number.

use Socket;

sub connectsocket
{
    my ($fd, $ipaddr, $port) = @_;
    my ($iaddr, $paddr);
    my $proto = getprotobyname('tcp');

    $iaddr = inet_aton($ipaddr);
    $paddr = sockaddr_in($port, $iaddr); 
    socket($fd, PF_INET, SOCK_STREAM, $proto) or die "Can't create $fd TCP socket $!\n"; 
    connect($fd, $paddr) or die "Can't connect to $ipaddr $port!\n";
}

my ($rin, $win, $ein);
my ($rout, $wout, $eout);
my @partial_line = ("","","","");

# Note sysread from a TCP socket does not guarantee 1 line per read or even
# complete lines per read. sysread may return line fragments and more or less 
# than 1 line. The following code does not handle all cases but works. 
# Scanning character by character for newline is the easiest but least
# efficient way to handle all possible cases.

sub printlines
{
    my ($fd, $id, $hostname) = @_;
    my ($bytesIn, $buf);
    my (@lines, $aLine);
    my ($i, $NLterm);

    if (defined($bytesIn = sysread($fd, $buf, 1024))) {
        if ($bytesIn == 0) {
            print "sysread $hostname EOF\n";
            vec($rin, fileno($fd), 1) = 0;
        }
        else {
            $NLterm = chomp($buf);  # NLterm >0 if '\n' at end of $buf
            @lines = split(/\n/, $buf);
            for ($i = 0; $i < @lines; $i++) {
                $aLine = $lines[$i];
                if (($i == $#lines) && !$NLterm) {
                    # If the last line is not \n terminated save it for next time
                    $partial_line[$id] = $partial_line[$id] . $aLine;
                    print "<";
                }
                print "$id: ";
                if (($i == 0) && $partial_line[$id] ne "") {
                    print $partial_line[$id];
                    $partial_line[$id] = "";
                    print ">";
                }
                #print "!" if ($i);
                print "$aLine\n";
            }
        }
    }
    else {
        print "sysread $hostname $!\n"
    }
}

# Read input from network sockets. Using select() requires using unbuffered
# read via sysread().
sub select2sock
{
    my ($ipaddr1, $port1, $ipaddr2, $port2, $ipaddr3, $port3) = @_;
    my ($iaddr, $paddr);
    my ($rin, $win, $ein);
    my ($nfound, $timeleft, $rout, $wout, $eout, $timeout);

    $rin = $win = $ein = '';

    connectsocket(CM15A1, $ipaddr1, $port1);
    vec($rin, fileno(CM15A1), 1) = 1;
    connectsocket(CM15A2, $ipaddr2, $port2);
    vec($rin, fileno(CM15A2), 1) = 1;
    connectsocket(CM19A1, $ipaddr3, $port3);
    vec($rin, fileno(CM19A1), 1) = 1;

    $ein = $win = $rin;

    $win = undef;
    while (1) {
        $timeout = undef;
        $nfound = select($rout=$rin, $wout=$win, $eout=$ein, $timeout);
        if ($nfound) {
            if (defined($rout)) {
                if (vec($rout, fileno(CM15A1), 1)) {
                    printlines(CM15A1, 1, "biceff");
                }
                if (vec($rout, fileno(CM15A2), 1)) {
                    printlines(CM15A2, 2, "dstar2");
                }
                if (vec($rout, fileno(CM19A1), 1)) {
                    printlines(CM19A1, 3, "dstar4");
                }
            }
            if (defined($wout)) {
                if (vec($wout, fileno(CM15A1), 1)) {
                    print "syswrite biceff\n";
                }
                if (vec($wout, fileno(CM15A2), 1)) {
                    print "syswrite dstar2\n";
                }
                if (vec($wout, fileno(CM19A1), 1)) {
                    print "syswrite dstar4\n";
                }
            }
            if (defined($eout)) {
                if (vec($eout, fileno(CM15A1), 1)) {
                    print "error on biceff\n";
                    last;
                }
                if (vec($eout, fileno(CM15A2), 1)) {
                    print "error on dstar2\n";
                    last;
                }
                if (vec($eout, fileno(CM19A1), 1)) {
                    print "error on dstar4\n";
                    last;
                }
            }
        }
    }
}

# 192.168.1.254, .23, and .26 are Linux hosts running mochad. 1099 is the 
# default port number mochad listens on.
select2sock("192.168.1.254", 1099, "192.168.1.24", 1099, "192.168.1.26", 1099);
