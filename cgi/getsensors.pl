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

#use Socket;

sub getsensors
{
    my ($ipaddr, $port, $hostname) = @_;
    my $sockaddr_in;
    my @quads;
    my $aline;
    my $devstatus;
    my ($mmdd, $hhmmss, $junk, $housechar, $unitstatus, $unit, $ustatus);
    my ($secaddr, $secstatus, $secmmss, $rowcolor);
    my (@fields, @unitrow, $i);
    my (%sensornames);

    %sensornames = (
        # X10 security device address => Description
        # You must use security devices address of your own. These are random
        # addresses.
        '11'     => 'Office Remote',
        '159E80' => 'Front',
        '168980' => 'Car Remote',
        '219D80' => 'Lanai',
        '264D00' => 'Living 1', 
        '34CB80' => 'Master Remote',
        '6BAA00' => 'Test Switch',
        '7FCA00' => 'Garage 1',
        '9D7A80' => 'Bedroom 2',
        'C61B00' => 'Bedroom 1',
        'F27E00' => 'Kitchen 1',
        'A1'     => 'Garage',
        'A2'     => 'Daytime'
    );

    #print "netcat($ipaddr, $port, $senddata)\n";

    my $proto = getprotobyname('tcp'); 

# convert ip and port addresses
    my $iaddr = inet_aton($ipaddr);
    my $paddr = sockaddr_in($port, $iaddr); 
# create the socket, connect to the port 
    socket(SOCKET, PF_INET, SOCK_STREAM, $proto) or die "socket() $!"; 
    connect(SOCKET, $paddr) or die "connect() $!"; 

    send(SOCKET, "ST\n", 0);
    $devstatus = 0;
    $mmdd = $hhmmss = '';
    while ($aline = <SOCKET>) {
        last if ($aline =~ /End status/);
        if ($devstatus == 0) {
            if ($aline =~ /Device status/) {
                $devstatus = 1;
                print '<P><table border="1">';
                print "<tr><th>H</th><th>1</th><th>2</th><th>3</th><th>4</th><th>5</th><th>6</th><th>7</th><th>8</th><th>9</th><th>10</th><th>11</th><th>12</th><th>13</th><th>14</th><th>15</th><th>16</th></tr>";
            }
        }
        elsif ($devstatus == 1) {
            if ($aline =~ /Security sensor/) {
                $devstatus = 2;
                print '</table></P>';
                print '<P><table border="1">';
                print "<tr><th>Sensor</th><th>Status</th><th>Last seen (min:sec)</th></tr>";
             }
            else {
                #06/21 18:28:54 House A: 1=1,2=0 
                #print $aline, "<BR>"
                ($mmdd, $hhmmss, $junk, $housechar, $unitstatus) = split(/ +/, $aline);
                $housechar =~ s/://;
                #print "$housechar $unitstatus\n";
                #print "<P>";
                #@fields = split(/,/, $unitstatus);
                #foreach $f (@fields) {
                #    ($unit, $ustatus) = split(/\=/, $f);
                #    print "$housechar$unit ";
                #    print "ON  " if ($ustatus == "1");
                #    print "OFF " if ($ustatus == "0");
                #}
                #print "</P>\n";
                print "<tr><td>$housechar</td>";
                for ($i = 1; $i <= 16; $i++) { $unitrow[$i] = "?"; }
                @fields = split(/,/, $unitstatus);
                foreach $f (@fields) {
                    ($unit, $ustatus) = split(/\=/, $f);
                    #print "$f $unit $ustatus<BR>";
                    if ($ustatus eq "1" || $ustatus == 1) {
                        #print "$housechar $unit ON<BR>";
                        $unitrow[$unit] = "<font color=\"#FF0000\">*</font>";
                    }
                    elsif ($ustatus eq "0" || $ustatus == 0) {
                        #print "$housechar $unit ON<BR>";
                        $unitrow[$unit] = "-";
                    }
                }
                for ($i = 1; $i <= 16; $i++) {
                    print "<td>$unitrow[$i]</td>";
                }
                print "</tr>";
            }
        }
        else {
            # 11/26 23:48:40 Sensor addr: 34CB80 Last: 00:07 (C6)Lights_Off (KR10A) 
            ($mmdd, $hhmmss, $junk, $junk, $secaddr, $junk, $secmmss, $secstatus) = split(/ +/, $aline);
            #print "Sensor $secaddr $secstatus since $sechhmm<BR>";
            if (exists($sensornames{$secaddr})) {
                $secaddr = $sensornames{$secaddr};
                if ($secstatus =~ /alert/) {
                    $rowcolor = "#FF0000";  # red
                }
                else {
                    $rowcolor = "#008000";  # green
                }
                print "<tr><td><font color=\"$rowcolor\">$secaddr</font></td><td><font color=\"$rowcolor\">$secstatus</font></td><td><font color=\"$rowcolor\">$secmmss</font></td></tr>";
            }
        }
    }
    print '</table></P>';
    print "<P>Last update: $mmdd $hhmmss from $hostname</P>\n";
    close SOCKET or die "close: $!";
}
1;

#getsensors("192.168.1.12", 1099);
