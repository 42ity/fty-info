/*  =========================================================================
    fty_info_rc0_runonce - Run once actor to update rackcontroller-0 (SN, ...)

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

#pragma once
#include <czmq.h>

typedef struct _fty_info_rc0_runonce_t fty_info_rc0_runonce_t;

//  Create a new fty_info_rc0_runonce
fty_info_rc0_runonce_t* fty_info_rc0_runonce_new(char* name);

//  Destroy the fty_info_rc0_runonce
void fty_info_rc0_runonce_destroy(fty_info_rc0_runonce_t** self_p);

//  Run once agent to update rackcontroller-0 first time it is created
void fty_info_rc0_runonce(zsock_t* pipe, void* args);
