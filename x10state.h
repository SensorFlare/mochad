/*
 * Copyright 2010-2011 Brian Uechi <buasst@gmail.com>
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

void hua_sec_init(void);

void hua_sec_event(unsigned char *secaddr, unsigned int funcint, 
        unsigned int secaddr8);

void hua_add(int house, int unit);

void hua_func_all_on(int house);

void hua_func_all_off(int house);

void hua_func_on(int house);

void hua_func_off(int house);

void hua_show(int fd);
