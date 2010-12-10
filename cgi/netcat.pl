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

sub netcat
{
    my ($ipaddr, $port, $senddata) = @_;
    my $sockaddr_in;
    my @quads;
    my $line;
    my $proto = getprotobyname('tcp'); 

# convert ip and port addresses
    my $iaddr = inet_aton($ipaddr);
    my $paddr = sockaddr_in($port, $iaddr); 
# create the socket, connect to the port 
    socket(SOCKET, PF_INET, SOCK_STREAM, $proto) or die "socket() $!"; 
    connect(SOCKET, $paddr) or die "connect() $!"; 

    send(SOCKET, $senddata, 0);
    close SOCKET or die "close: $!";
}
1;

#netcat("192.168.1.12", 1099, "pl a1 off\r");
