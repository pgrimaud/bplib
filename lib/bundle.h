/************************************************************************
 * File: bundle.h
 *
 *  Copyright 2019 United States Government as represented by the 
 *  Administrator of the National Aeronautics and Space Administration. 
 *  All Other Rights Reserved.  
 *
 *  This software was created at NASA's Goddard Space Flight Center.
 *  This software is governed by the NASA Open Source Agreement and may be 
 *  used, distributed and modified only pursuant to the terms of that 
 *  agreement.
 *
 * Maintainer(s):
 *  Joe-Paul Swinski, Code 582 NASA GSFC
 *
 *************************************************************************/

#ifndef _bundle_h_
#define _bundle_h_

/******************************************************************************
 INCLUDES
 ******************************************************************************/

#include "bplib.h"
#include "bundle_types.h"

/******************************************************************************
 PROTOTYPES
 ******************************************************************************/

int     bundle_initialize   (bp_bundle_t* bundle, bp_route_t route, bp_attr_t* attributes, bp_generate_t generate, void* parm, uint16_t* flags);
int     bundle_uninitialize (bp_bundle_t* bundle);
int     bundle_generate     (bp_bundle_t* bundle, uint8_t* pay, int pay_size, int timeout, uint16_t* flags);
int     bundle_forward      (bp_bundle_t* bundle, uint8_t* pay, int pay_size, int timeout, uint16_t* flags);
int     bundle_receive      (bp_bundle_t* bundle, uint8_t* block, int block_size, bp_custodian_t* custodian, uint16_t* flags);
int     bundle_update       (bp_bundle_data_t* data, bp_val_t cid, uint16_t* flags);
int     bundle_routeinfo    (void* bundle, int size, bp_route_t* route);

#endif  /* _bundle_h_ */
