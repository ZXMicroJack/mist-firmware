/*
  This file is part of MiST-firmware

  MiST-firmware is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  MiST-firmware is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

unsigned char settings_load(char global);
unsigned char settings_save(char global);

extern uint8_t joymenu_select;
extern uint8_t joymenu_start;

#ifndef COREEXTOTHER
#define COREEXTOTHER
#endif

#ifndef MIST_ROOT
#define MIST_ROOT "/"
#endif

#ifndef COREEXT
#define COREEXT "RBF"
#endif

