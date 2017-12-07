/*  =========================================================================
    topologyresolver - Class for asset location recursive resolving

    Copyright (C) 2014 - 2017 Eaton

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

#ifndef TOPOLOGYRESOLVER_H_INCLUDED
#define TOPOLOGYRESOLVER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new topologyresolver
FTY_INFO_PRIVATE topologyresolver_t *
    topologyresolver_new (const char *iname);

//  Destroy the topologyresolver
FTY_INFO_PRIVATE void
    topologyresolver_destroy (topologyresolver_t **self_p);

//  Set endpoint for Malamute client
FTY_INFO_PRIVATE void
    topologyresolver_set_endpoint (topologyresolver_t *self, const char *endpoint);

//  get RC internal name
FTY_INFO_PRIVATE char *
    topologyresolver_id (topologyresolver_t *self);

// Return URI of asset for this topologyresolver
FTY_INFO_PRIVATE char *
    topologyresolver_to_rc_name_uri (topologyresolver_t *self);

//  Give topology resolver one asset information
FTY_INFO_PRIVATE bool
    topologyresolver_asset (topologyresolver_t *self, fty_proto_t *message);

//  Return URI of the asset's parent
FTY_INFO_PRIVATE char *
    topologyresolver_to_parent_uri (topologyresolver_t *self);

//  Return user-friendly name of the asset
FTY_INFO_PRIVATE char *
    topologyresolver_to_rc_name (topologyresolver_t *self);

//  Return description of the asset
FTY_INFO_PRIVATE char *
    topologyresolver_to_description (topologyresolver_t *self);

//  Return contact of the asset
FTY_INFO_PRIVATE char *
    topologyresolver_to_contact (topologyresolver_t *self);

//  Return topology as string of friedly names (or NULL if incomplete)
FTY_INFO_PRIVATE char *
    topologyresolver_to_string (topologyresolver_t *self, const char *separator = "/");

//  Return zlist of inames starting with asset up to DC
//  Empty list is returned if the topology is incomplete yet
FTY_INFO_PRIVATE zlistx_t *
    topologyresolver_to_list (topologyresolver_t *self);

// Selftest for this class
FTY_INFO_PRIVATE void
    topologyresolver_test (bool verbose);


//  @end

#ifdef __cplusplus
}
#endif

#endif
