/************************************************************************
 * File: crc.h
 *
 *  Copyright 2019 United States Government as represented by the
 *  Administrator of the National Aeronautics and Space Administration.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * Maintainer(s):
 *  Joe-Paul Swinski, Code 582 NASA GSFC
 *
 *************************************************************************/

#ifndef _BPLIB_CRC_H_
#define _BPLIB_CRC_H_

/******************************************************************************
 INCLUDES
 ******************************************************************************/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/******************************************************************************
 DEFINES
 ******************************************************************************/

#define BYTE_COMBOS 256 /* Number of different possible bytes. */
#define BP_CRC_INIT_SUCCESS 0 /* A sucessful return when initing a crc_parameters_t. */
#define BP_CRC_INIT_FAIL_INVALID_LENGTH 1 /* The provided crc param struct has invalid length. */

/******************************************************************************
 TYPEDEFS
 ******************************************************************************/

/* Parameters specific to calculating 16 bit crc parameters. */
typedef struct crc16_parameters
{
    uint16_t generator_polynomial;   /* The generator polynomial used to compute the CRC. */
    uint16_t initial_value;          /* The value used to initialize a CRC. */
    uint16_t final_xor;              /* The final value to xor with the crc before returning. */
    uint16_t check_value;            /* The crc resulting from the input string "123456789". */
    uint16_t xor_table[BYTE_COMBOS]; /* A ptr to a table with the precomputed XOR values. */
} crc16_parameters_t;

/* Parameters specific to calculating 32 bit crcs. */
typedef struct crc32_parameters
{
    uint32_t generator_polynomial;   /* The generator polynomial used to compute the CRC. */
    uint32_t initial_value;          /* The value used to initialize a CRC. */
    uint32_t final_xor;              /* The final value to xor with the crc before returning. */
    uint32_t check_value;            /* The crc resulting from the input string "123456789". */
    uint32_t xor_table[BYTE_COMBOS]; /* A ptr to a table with the precomputed XOR values. */
} crc32_parameters_t;

/* Standard parameters for calculating a CRC. */
typedef struct crc_parameters
{
    char* name;                 /* Name of the CRC. */
    int length;                 /* The number of bits in the CRC. */
    bool should_reflect_input;  /* Whether to reflect the bits of the input bytes. */
    bool should_reflect_output; /* Whether to reflect the bits of the output crc. */
    /* Parameters specific to crc implementations of various lengths. The field that is populated
       within this union should directly coincide with the length member.
       Ex: If length == 16 crc16 should be popualted in this union below. */
    union {
        crc16_parameters_t crc16;
        crc32_parameters_t crc32;
    } n_bit_params;
} crc_parameters_t;

/******************************************************************************
 PROTOTYPES
 ******************************************************************************/
/* Create a lookup tables for the provided crc. */
int crc_init(crc_parameters_t* params);
/* Calculates a crc from an initialized set of params on data. */
uint32_t crc_get(const uint8_t* data, const uint32_t length, const crc_parameters_t* params);
#endif /* _BPLIB_CRC_H_ */