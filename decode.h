/*
 * Copyright 2010 Brian Uechi <buasst@gmail.com>
 *
 * This file is part of mochad.
 *
 * mochad is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * mochad is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mochad.  If not, see <http://www.gnu.org/licenses/>.
 */

const char *findSecEventName(unsigned char secev);

const char *findSecRemoteKeyName(unsigned char secev);

int findCamRemoteCommand(const char *keyname);

void cm15a_decode_plc(int fd, unsigned char *buf, size_t len);
    
void cm15a_decode_rf(int fd, unsigned char *buf, unsigned int len);

void cm15a_decode(int fd, unsigned char *buf, unsigned int len);

