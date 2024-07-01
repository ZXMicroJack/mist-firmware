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

#include <string.h>
#include "ini_parser.h"
#include "settings.h"
#include "user_io.h"
#include "usb/joymapping.h"

extern char s[FF_LFN_BUF + 1];

static uint64_t status;

#ifdef ZXUNO
static uint8_t boot_menu = 1;
#endif

// core ini sections
const ini_section_t core_ini_sections[] = {
	{1, "CFG"}
};

// core ini vars
const ini_var_t core_ini_global_vars[] = {
	{"JOYSTICK_REMAP", (void*)virtual_joystick_remap, CUSTOM_HANDLER, 0, 0, 1}
#ifdef ZXUNO
  ,{"BOOT_MENU", (void*)(&(boot_menu)), UINT8, 0, 1, 1}
#endif
};

const ini_var_t core_ini_local_vars[] = {
	{"STATUS", (void*)(&status), UINT64, 0, 0xFFFFFFFFFFFFFFFF, 1},
	{"JOYSTICK_REMAP", (void*)virtual_joystick_remap, CUSTOM_HANDLER, 0, 0, 1}
};

static char settings_setup(ini_cfg_t *ini, char global) {
	if(global) {
		ini->filename = MIST_ROOT "/MISTCFG.INI";
	} else {
		if(user_io_core_type() == CORE_TYPE_8BIT &&
		   !user_io_create_config_name(s, "CFG", CONFIG_ROOT)) {
			ini->filename = s;
		} else {
			return 0;
		}
	}
	ini->sections = core_ini_sections;
	ini->vars = global ? core_ini_global_vars : core_ini_local_vars;
	ini->nsections = (int)(sizeof(core_ini_sections) / sizeof(ini_section_t));
	ini->nvars = global ? (int)(sizeof(core_ini_global_vars) / sizeof(ini_var_t)) :
	                      (int)(sizeof(core_ini_local_vars) / sizeof(ini_var_t));
	return 1;
}

unsigned char settings_load(char global)
{
	ini_cfg_t core_ini_cfg;
	if (!settings_setup(&core_ini_cfg, global))
		return 0;

	ini_parse(&core_ini_cfg, 0, global ? 1 : 2);
	if (!global) user_io_8bit_set_status(status, ~1);
	return 1;
};

unsigned char settings_save(char global)
{
	ini_cfg_t core_ini_cfg;
	if (!settings_setup(&core_ini_cfg, global))
		return 0;
	if (!global) status = user_io_8bit_set_status(0,0);
	virtual_joystick_remap_init(true);
	ini_save(&core_ini_cfg, global ? 1 : 2);
	return 1;
}

#ifdef ZXUNO
// core ini vars
const ini_var_t board_ini_vars[] = {
  {"BOOT_MENU", (void*)(&(boot_menu)), UINT8, 0, 1, 1}
};

const ini_section_t board_ini_sections[] = {
  {1, "BOARD"},
};

// mist ini config
const ini_cfg_t board_ini_cfg = {
  MIST_ROOT"/BOARD.INI",
  board_ini_sections,
  board_ini_vars,
  (int)(sizeof(board_ini_sections) / sizeof(ini_section_t)),
  (int)(sizeof(board_ini_vars)     / sizeof(ini_var_t))
};


void settings_board_load() {
  boot_menu = 1;
  ini_parse(&board_ini_cfg, 0, 0);
}

void settings_board_save() {
  ini_save(&board_ini_cfg, 0);
}

uint8_t settings_boot_menu() {
  return boot_menu;
}

void settings_set_boot(uint8_t state) {
  boot_menu = state;
}
#endif
