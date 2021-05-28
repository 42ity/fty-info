/*  =========================================================================
    topologyresolver - Class for asset location recursive resolving

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
#include <fty_proto.h>

typedef struct _topologyresolver_t topologyresolver_t;

//  Create a new topologyresolver
topologyresolver_t* topologyresolver_new(const char* iname);

//  Destroy the topologyresolver
void topologyresolver_destroy(topologyresolver_t** self_p);

//  Set endpoint for Malamute client
void topologyresolver_set_endpoint(topologyresolver_t* self, const char* endpoint);

//  get RC internal name
char* topologyresolver_id(topologyresolver_t* self);

// Return URI of asset for this topologyresolver
char* topologyresolver_to_rc_name_uri(topologyresolver_t* self);

//  Give topology resolver one asset information
bool topologyresolver_asset(topologyresolver_t* self, fty_proto_t* message);

//  Return URI of the asset's parent
char* topologyresolver_to_parent_uri(topologyresolver_t* self);

//  Return user-friendly name of the asset
char* topologyresolver_to_rc_name(topologyresolver_t* self);

//  Return description of the asset
char* topologyresolver_to_description(topologyresolver_t* self);

//  Return contact of the asset
char* topologyresolver_to_contact(topologyresolver_t* self);

//  Return topology as string of friedly names (or NULL if incomplete)
char* topologyresolver_to_string(topologyresolver_t* self, const char* separator = "/");

//  Return zlist of inames starting with asset up to DC
//  Empty list is returned if the topology is incomplete yet
zlistx_t* topologyresolver_to_list(topologyresolver_t* self);
