/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mutator_aux.h"
#include "fido.h"
#include "fido/credman.h"

#include "../openbsd-compat/openbsd-compat.h"

#define TAG_META_WIRE_DATA	0x01
#define TAG_RP_WIRE_DATA	0x02
#define TAG_RK_WIRE_DATA	0x03
#define TAG_DEL_WIRE_DATA	0x04
#define TAG_CRED_ID		0x05
#define TAG_PIN			0x06
#define TAG_RP_ID		0x07
#define TAG_SEED		0x08

/* Parameter set defining a FIDO2 credential management operation. */
struct param {
	char		pin[MAXSTR];
	char		rp_id[MAXSTR];
	int		seed;
	struct blob	cred_id;
	struct blob	del_wire_data;
	struct blob	meta_wire_data;
	struct blob	rk_wire_data;
	struct blob	rp_wire_data;
};

/* Example parameters. */
static const uint8_t dummy_cred_id[] = {
	0x4f, 0x72, 0x98, 0x42, 0x4a, 0xe1, 0x17, 0xa5,
	0x85, 0xa0, 0xef, 0x3b, 0x11, 0x24, 0x4a, 0x3d,
};
static const char dummy_pin[] = "[n#899:~m";
static const char dummy_rp_id[] = "yubico.com";

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'getCredsMetadata' credential management command.
 */
static const uint8_t dummy_meta_wire_data[] = {
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0xc5,
	0xb7, 0x89, 0xba, 0x8d, 0x5f, 0x94, 0x1b, 0x00,
	0x12, 0x00, 0x04, 0x02, 0x00, 0x04, 0x05, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x12, 0x00, 0x04, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0x93, 0xc5, 0x64,
	0x71, 0xe9, 0xd1, 0xb8, 0xed, 0xf6, 0xd5, 0xf3,
	0xa7, 0xd5, 0x96, 0x70, 0xbb, 0xd5, 0x20, 0xa1,
	0xa3, 0xd3, 0x93, 0x4c, 0x5c, 0x20, 0x5c, 0x22,
	0xeb, 0xb0, 0x6a, 0x27, 0x59, 0x22, 0x58, 0x20,
	0x63, 0x02, 0x33, 0xa8, 0xed, 0x3c, 0xbc, 0xe9,
	0x00, 0x12, 0x00, 0x04, 0x00, 0xda, 0x44, 0xf5,
	0xed, 0xda, 0xe6, 0xa4, 0xad, 0x3f, 0x9e, 0xf8,
	0x50, 0x8d, 0x01, 0x47, 0x6c, 0x4e, 0x72, 0xa4,
	0x04, 0x13, 0xa8, 0x65, 0x97, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x12, 0x00, 0x04, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0x6f, 0x11, 0x96, 0x21, 0x92,
	0x52, 0xf1, 0x6b, 0xd4, 0x2c, 0xe3, 0xf8, 0xc9,
	0x8c, 0x47, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x12, 0x00, 0x04, 0x90, 0x00, 0x07, 0x00,
	0xa2, 0x01, 0x00, 0x02, 0x18, 0x19, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'enumerateRPsBegin' credential management command.
 */
static const uint8_t dummy_rp_wire_data[] = {
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0x87,
	0xbf, 0xc6, 0x7f, 0x36, 0xf5, 0xe2, 0x49, 0x00,
	0x15, 0x00, 0x02, 0x02, 0x00, 0x04, 0x05, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x02, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0x12, 0xc1, 0x81,
	0x6b, 0x92, 0x6a, 0x56, 0x05, 0xfe, 0xdb, 0xab,
	0x90, 0x2f, 0x57, 0x0b, 0x3d, 0x85, 0x3e, 0x3f,
	0xbc, 0xe5, 0xd3, 0xb6, 0x86, 0xdf, 0x10, 0x43,
	0xc2, 0xaf, 0x87, 0x34, 0x0e, 0x22, 0x58, 0x20,
	0xd3, 0x0f, 0x7e, 0x5d, 0x10, 0x33, 0x57, 0x24,
	0x00, 0x15, 0x00, 0x02, 0x00, 0x6e, 0x90, 0x58,
	0x61, 0x2a, 0xd2, 0xc2, 0x1e, 0x08, 0xea, 0x91,
	0xcb, 0x44, 0x66, 0x73, 0x29, 0x92, 0x29, 0x59,
	0x91, 0xa3, 0x4d, 0x2c, 0xbb, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x02, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0x6d, 0x95, 0x0e, 0x73, 0x78,
	0x46, 0x13, 0x2e, 0x07, 0xbf, 0xeb, 0x61, 0x31,
	0x37, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x02, 0x90, 0x00, 0x37, 0x00,
	0xa3, 0x03, 0xa1, 0x62, 0x69, 0x64, 0x6a, 0x79,
	0x75, 0x62, 0x69, 0x63, 0x6f, 0x2e, 0x63, 0x6f,
	0x6d, 0x04, 0x58, 0x20, 0x37, 0x82, 0x09, 0xb7,
	0x2d, 0xef, 0xcb, 0xa9, 0x1d, 0xcb, 0xf8, 0x54,
	0xed, 0xb4, 0xda, 0xa6, 0x48, 0x82, 0x8a, 0x2c,
	0xbd, 0x18, 0x0a, 0xfc, 0x77, 0xa7, 0x44, 0x34,
	0x65, 0x5a, 0x1c, 0x7d, 0x05, 0x03, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x02, 0x90, 0x00, 0x36, 0x00,
	0xa2, 0x03, 0xa1, 0x62, 0x69, 0x64, 0x6b, 0x79,
	0x75, 0x62, 0x69, 0x6b, 0x65, 0x79, 0x2e, 0x6f,
	0x72, 0x67, 0x04, 0x58, 0x20, 0x12, 0x6b, 0xba,
	0x6a, 0x2d, 0x7a, 0x81, 0x84, 0x25, 0x7b, 0x74,
	0xdd, 0x1d, 0xdd, 0x46, 0xb6, 0x2a, 0x8c, 0xa2,
	0xa7, 0x83, 0xfe, 0xdb, 0x5b, 0x19, 0x48, 0x73,
	0x55, 0xb7, 0xe3, 0x46, 0x09, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x02, 0x90, 0x00, 0x37, 0x00,
	0xa2, 0x03, 0xa1, 0x62, 0x69, 0x64, 0x6c, 0x77,
	0x65, 0x62, 0x61, 0x75, 0x74, 0x68, 0x6e, 0x2e,
	0x64, 0x65, 0x76, 0x04, 0x58, 0x20, 0xd6, 0x32,
	0x7d, 0x8c, 0x6a, 0x5d, 0xe6, 0xae, 0x0e, 0x33,
	0xd0, 0xa3, 0x31, 0xfb, 0x67, 0x77, 0xb9, 0x4e,
	0xf4, 0x73, 0x19, 0xfe, 0x7e, 0xfd, 0xfa, 0x82,
	0x70, 0x8e, 0x1f, 0xbb, 0xa2, 0x55, 0x00, 0x00,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'enumerateCredentialsBegin' credential management command.
 */
static const uint8_t dummy_rk_wire_data[] = {
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0x35,
	0x3b, 0x34, 0xb9, 0xcb, 0xeb, 0x40, 0x55, 0x00,
	0x15, 0x00, 0x04, 0x02, 0x00, 0x04, 0x05, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x04, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0x12, 0xc1, 0x81,
	0x6b, 0x92, 0x6a, 0x56, 0x05, 0xfe, 0xdb, 0xab,
	0x90, 0x2f, 0x57, 0x0b, 0x3d, 0x85, 0x3e, 0x3f,
	0xbc, 0xe5, 0xd3, 0xb6, 0x86, 0xdf, 0x10, 0x43,
	0xc2, 0xaf, 0x87, 0x34, 0x0e, 0x22, 0x58, 0x20,
	0xd3, 0x0f, 0x7e, 0x5d, 0x10, 0x33, 0x57, 0x24,
	0x00, 0x15, 0x00, 0x04, 0x00, 0x6e, 0x90, 0x58,
	0x61, 0x2a, 0xd2, 0xc2, 0x1e, 0x08, 0xea, 0x91,
	0xcb, 0x44, 0x66, 0x73, 0x29, 0x92, 0x29, 0x59,
	0x91, 0xa3, 0x4d, 0x2c, 0xbb, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x04, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0x1b, 0xf0, 0x01, 0x0d, 0x32,
	0xee, 0x28, 0xa4, 0x5a, 0x7f, 0x56, 0x5b, 0x28,
	0xfd, 0x1f, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x04, 0x90, 0x00, 0xc5, 0x00,
	0xa5, 0x06, 0xa3, 0x62, 0x69, 0x64, 0x58, 0x20,
	0xe4, 0xe1, 0x06, 0x31, 0xde, 0x00, 0x0f, 0x4f,
	0x12, 0x6e, 0xc9, 0x68, 0x2d, 0x43, 0x3f, 0xf1,
	0x02, 0x2c, 0x6e, 0xe6, 0x96, 0x10, 0xbf, 0x73,
	0x35, 0xc9, 0x20, 0x27, 0x06, 0xba, 0x39, 0x09,
	0x64, 0x6e, 0x61, 0x6d, 0x65, 0x6a, 0x62, 0x6f,
	0x62, 0x20, 0x62, 0x61, 0x6e, 0x61, 0x6e, 0x61,
	0x00, 0x15, 0x00, 0x04, 0x00, 0x6b, 0x64, 0x69,
	0x73, 0x70, 0x6c, 0x61, 0x79, 0x4e, 0x61, 0x6d,
	0x65, 0x67, 0x62, 0x62, 0x61, 0x6e, 0x61, 0x6e,
	0x61, 0x07, 0xa2, 0x62, 0x69, 0x64, 0x50, 0x19,
	0xf7, 0x78, 0x0c, 0xa0, 0xbc, 0xb9, 0xa6, 0xd5,
	0x1e, 0xd7, 0x87, 0xfb, 0x6c, 0x80, 0x03, 0x64,
	0x74, 0x79, 0x70, 0x65, 0x6a, 0x70, 0x75, 0x62,
	0x6c, 0x69, 0x63, 0x2d, 0x6b, 0x65, 0x79, 0x08,
	0x00, 0x15, 0x00, 0x04, 0x01, 0xa5, 0x01, 0x02,
	0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20, 0x81,
	0x6c, 0xdd, 0x8c, 0x8f, 0x8c, 0xc8, 0x43, 0xa7,
	0xbb, 0x79, 0x51, 0x09, 0xb1, 0xdf, 0xbe, 0xc4,
	0xa5, 0x54, 0x16, 0x9e, 0x58, 0x56, 0xb3, 0x0b,
	0x34, 0x4f, 0xa5, 0x6c, 0x05, 0xa2, 0x21, 0x22,
	0x58, 0x20, 0xcd, 0xc2, 0x0c, 0x99, 0x83, 0x5a,
	0x61, 0x73, 0xd8, 0xe0, 0x74, 0x23, 0x46, 0x64,
	0x00, 0x15, 0x00, 0x04, 0x02, 0x39, 0x4c, 0xb0,
	0xf4, 0x6c, 0x0a, 0x37, 0x72, 0xaa, 0xa8, 0xea,
	0x58, 0xd3, 0xd4, 0xe0, 0x51, 0xb2, 0x28, 0x09,
	0x05, 0x0a, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x04, 0x90, 0x00, 0xa0, 0x00,
	0xa4, 0x06, 0xa3, 0x62, 0x69, 0x64, 0x58, 0x20,
	0x56, 0xa1, 0x3c, 0x06, 0x2b, 0xad, 0xa2, 0x21,
	0x7d, 0xcd, 0x91, 0x08, 0x47, 0xa8, 0x8a, 0x06,
	0x06, 0xf6, 0x66, 0x91, 0xf6, 0xeb, 0x89, 0xe4,
	0xdf, 0x26, 0xbc, 0x46, 0x59, 0xc3, 0x7d, 0xc0,
	0x64, 0x6e, 0x61, 0x6d, 0x65, 0x6a, 0x62, 0x6f,
	0x62, 0x20, 0x62, 0x61, 0x6e, 0x61, 0x6e, 0x61,
	0x00, 0x15, 0x00, 0x04, 0x00, 0x6b, 0x64, 0x69,
	0x73, 0x70, 0x6c, 0x61, 0x79, 0x4e, 0x61, 0x6d,
	0x65, 0x67, 0x62, 0x62, 0x61, 0x6e, 0x61, 0x6e,
	0x61, 0x07, 0xa2, 0x62, 0x69, 0x64, 0x50, 0xd8,
	0x27, 0x4b, 0x25, 0xed, 0x19, 0xef, 0x11, 0xaf,
	0xa6, 0x89, 0x7b, 0x84, 0x50, 0xe7, 0x62, 0x64,
	0x74, 0x79, 0x70, 0x65, 0x6a, 0x70, 0x75, 0x62,
	0x6c, 0x69, 0x63, 0x2d, 0x6b, 0x65, 0x79, 0x08,
	0x00, 0x15, 0x00, 0x04, 0x01, 0xa4, 0x01, 0x01,
	0x03, 0x27, 0x20, 0x06, 0x21, 0x58, 0x20, 0x8d,
	0xfe, 0x45, 0xd5, 0x7d, 0xb6, 0x17, 0xab, 0x86,
	0x2d, 0x32, 0xf6, 0x85, 0xf0, 0x92, 0x76, 0xb7,
	0xce, 0x73, 0xca, 0x4e, 0x0e, 0xfd, 0xd5, 0xdb,
	0x2a, 0x1d, 0x55, 0x90, 0x96, 0x52, 0xc2, 0x0a,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x04, 0x90, 0x00, 0xa0, 0x00,
	0xa4, 0x06, 0xa3, 0x62, 0x69, 0x64, 0x58, 0x20,
	0x04, 0x0e, 0x0f, 0xa0, 0xcd, 0x60, 0x35, 0x9a,
	0xba, 0x47, 0x0c, 0x10, 0xb6, 0x82, 0x6e, 0x2f,
	0x66, 0xb9, 0xa7, 0xcf, 0xd8, 0x47, 0xb4, 0x3d,
	0xfd, 0x77, 0x1a, 0x38, 0x22, 0xa1, 0xda, 0xa5,
	0x64, 0x6e, 0x61, 0x6d, 0x65, 0x6a, 0x62, 0x6f,
	0x62, 0x20, 0x62, 0x61, 0x6e, 0x61, 0x6e, 0x61,
	0x00, 0x15, 0x00, 0x04, 0x00, 0x6b, 0x64, 0x69,
	0x73, 0x70, 0x6c, 0x61, 0x79, 0x4e, 0x61, 0x6d,
	0x65, 0x67, 0x62, 0x62, 0x61, 0x6e, 0x61, 0x6e,
	0x61, 0x07, 0xa2, 0x62, 0x69, 0x64, 0x50, 0x00,
	0x5d, 0xdf, 0xef, 0xe2, 0xf3, 0x06, 0xb2, 0xa5,
	0x46, 0x4d, 0x98, 0xbc, 0x14, 0x65, 0xc1, 0x64,
	0x74, 0x79, 0x70, 0x65, 0x6a, 0x70, 0x75, 0x62,
	0x6c, 0x69, 0x63, 0x2d, 0x6b, 0x65, 0x79, 0x08,
	0x00, 0x15, 0x00, 0x04, 0x01, 0xa4, 0x01, 0x01,
	0x03, 0x27, 0x20, 0x06, 0x21, 0x58, 0x20, 0x72,
	0x79, 0x14, 0x69, 0xdf, 0xcb, 0x64, 0x75, 0xee,
	0xd4, 0x45, 0x94, 0xbc, 0x48, 0x4d, 0x2a, 0x9f,
	0xc9, 0xf4, 0xb5, 0x1b, 0x05, 0xa6, 0x5b, 0x54,
	0x9a, 0xac, 0x6c, 0x2e, 0xc6, 0x90, 0x62, 0x0a,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x04, 0x90, 0x00, 0xc3, 0x00,
	0xa4, 0x06, 0xa3, 0x62, 0x69, 0x64, 0x58, 0x20,
	0xce, 0x32, 0xd8, 0x79, 0xdd, 0x86, 0xa2, 0x42,
	0x7c, 0xc3, 0xe1, 0x95, 0x12, 0x93, 0x1a, 0x03,
	0xe6, 0x70, 0xb8, 0xff, 0xcd, 0xa5, 0xdf, 0x15,
	0xfc, 0x88, 0x2a, 0xf5, 0x44, 0xf1, 0x33, 0x9c,
	0x64, 0x6e, 0x61, 0x6d, 0x65, 0x6a, 0x62, 0x6f,
	0x62, 0x20, 0x62, 0x61, 0x6e, 0x61, 0x6e, 0x61,
	0x00, 0x15, 0x00, 0x04, 0x00, 0x6b, 0x64, 0x69,
	0x73, 0x70, 0x6c, 0x61, 0x79, 0x4e, 0x61, 0x6d,
	0x65, 0x67, 0x62, 0x62, 0x61, 0x6e, 0x61, 0x6e,
	0x61, 0x07, 0xa2, 0x62, 0x69, 0x64, 0x50, 0x0a,
	0x26, 0x5b, 0x7e, 0x1a, 0x2a, 0xba, 0x70, 0x5f,
	0x18, 0x26, 0x14, 0xb2, 0x71, 0xca, 0x98, 0x64,
	0x74, 0x79, 0x70, 0x65, 0x6a, 0x70, 0x75, 0x62,
	0x6c, 0x69, 0x63, 0x2d, 0x6b, 0x65, 0x79, 0x08,
	0x00, 0x15, 0x00, 0x04, 0x01, 0xa5, 0x01, 0x02,
	0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20, 0x8b,
	0x48, 0xf0, 0x69, 0xfb, 0x22, 0xfb, 0xf3, 0x86,
	0x57, 0x7c, 0xdd, 0x82, 0x2c, 0x1c, 0x0c, 0xdc,
	0x27, 0xe2, 0x6a, 0x4c, 0x1a, 0x10, 0x04, 0x27,
	0x51, 0x3e, 0x2a, 0x9d, 0x3a, 0xb6, 0xb5, 0x22,
	0x58, 0x20, 0x70, 0xfe, 0x91, 0x67, 0x64, 0x53,
	0x63, 0x83, 0x72, 0x31, 0xe9, 0xe5, 0x20, 0xb7,
	0x00, 0x15, 0x00, 0x04, 0x02, 0xee, 0xc9, 0xfb,
	0x63, 0xd7, 0xe4, 0x76, 0x39, 0x80, 0x82, 0x74,
	0xb8, 0xfa, 0x67, 0xf5, 0x1b, 0x8f, 0xe0, 0x0a,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x04, 0x90, 0x00, 0xc3, 0x00,
	0xa4, 0x06, 0xa3, 0x62, 0x69, 0x64, 0x58, 0x20,
	0xf9, 0xa3, 0x67, 0xbf, 0x5e, 0x80, 0x95, 0xdb,
	0x4c, 0xc5, 0x8f, 0x65, 0x36, 0xc5, 0xaf, 0xdd,
	0x90, 0x2e, 0x62, 0x68, 0x67, 0x9c, 0xa2, 0x26,
	0x2f, 0x2a, 0xf9, 0x3a, 0xda, 0x15, 0xf2, 0x27,
	0x64, 0x6e, 0x61, 0x6d, 0x65, 0x6a, 0x62, 0x6f,
	0x62, 0x20, 0x62, 0x61, 0x6e, 0x61, 0x6e, 0x61,
	0x00, 0x15, 0x00, 0x04, 0x00, 0x6b, 0x64, 0x69,
	0x73, 0x70, 0x6c, 0x61, 0x79, 0x4e, 0x61, 0x6d,
	0x65, 0x67, 0x62, 0x62, 0x61, 0x6e, 0x61, 0x6e,
	0x61, 0x07, 0xa2, 0x62, 0x69, 0x64, 0x50, 0xfb,
	0xa6, 0xbe, 0xc1, 0x01, 0xf6, 0x7a, 0x81, 0xf9,
	0xcd, 0x6d, 0x20, 0x41, 0x7a, 0x1c, 0x40, 0x64,
	0x74, 0x79, 0x70, 0x65, 0x6a, 0x70, 0x75, 0x62,
	0x6c, 0x69, 0x63, 0x2d, 0x6b, 0x65, 0x79, 0x08,
	0x00, 0x15, 0x00, 0x04, 0x01, 0xa5, 0x01, 0x02,
	0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20, 0xda,
	0x2b, 0x53, 0xc3, 0xbe, 0x48, 0xf8, 0xab, 0xbd,
	0x06, 0x28, 0x46, 0xfa, 0x35, 0xab, 0xf9, 0xc5,
	0x2e, 0xfd, 0x3c, 0x38, 0x88, 0xb3, 0xe1, 0xa7,
	0xc5, 0xc6, 0xed, 0x72, 0x54, 0x37, 0x93, 0x22,
	0x58, 0x20, 0x12, 0x82, 0x32, 0x2d, 0xab, 0xbc,
	0x64, 0xb3, 0xed, 0xcc, 0xd5, 0x22, 0xec, 0x79,
	0x00, 0x15, 0x00, 0x04, 0x02, 0x4b, 0xe2, 0x4d,
	0x0c, 0x4b, 0x8d, 0x31, 0x4c, 0xb4, 0x0f, 0xd4,
	0xa9, 0xbe, 0x0c, 0xab, 0x9e, 0x0a, 0xc9, 0x0a,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'deleteCredential' credential management command.
 */
static const uint8_t dummy_del_wire_data[] = {
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0x8b,
	0xe1, 0xf0, 0x3a, 0x18, 0xa5, 0xda, 0x59, 0x00,
	0x15, 0x00, 0x05, 0x02, 0x00, 0x04, 0x05, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x05, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0x12, 0xc1, 0x81,
	0x6b, 0x92, 0x6a, 0x56, 0x05, 0xfe, 0xdb, 0xab,
	0x90, 0x2f, 0x57, 0x0b, 0x3d, 0x85, 0x3e, 0x3f,
	0xbc, 0xe5, 0xd3, 0xb6, 0x86, 0xdf, 0x10, 0x43,
	0xc2, 0xaf, 0x87, 0x34, 0x0e, 0x22, 0x58, 0x20,
	0xd3, 0x0f, 0x7e, 0x5d, 0x10, 0x33, 0x57, 0x24,
	0x00, 0x15, 0x00, 0x05, 0x00, 0x6e, 0x90, 0x58,
	0x61, 0x2a, 0xd2, 0xc2, 0x1e, 0x08, 0xea, 0x91,
	0xcb, 0x44, 0x66, 0x73, 0x29, 0x92, 0x29, 0x59,
	0x91, 0xa3, 0x4d, 0x2c, 0xbb, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x05, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0x33, 0xf1, 0x3b, 0xde, 0x1e,
	0xa5, 0xd1, 0xbf, 0xf6, 0x5d, 0x63, 0xb6, 0xfc,
	0xd2, 0x24, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x15, 0x00, 0x05, 0x90, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int    LLVMFuzzerTestOneInput(const uint8_t *, size_t);
size_t LLVMFuzzerCustomMutator(uint8_t *, size_t, size_t, unsigned int);

static int
unpack(const uint8_t *ptr, size_t len, struct param *p) NO_MSAN
{
	uint8_t **pp = (void *)&ptr;

	if (unpack_string(TAG_PIN, pp, &len, p->pin) < 0 ||
	    unpack_string(TAG_RP_ID, pp, &len, p->rp_id) < 0 ||
	    unpack_blob(TAG_CRED_ID, pp, &len, &p->cred_id) < 0 ||
	    unpack_blob(TAG_META_WIRE_DATA, pp, &len, &p->meta_wire_data) < 0 ||
	    unpack_blob(TAG_RP_WIRE_DATA, pp, &len, &p->rp_wire_data) < 0 ||
	    unpack_blob(TAG_RK_WIRE_DATA, pp, &len, &p->rk_wire_data) < 0 ||
	    unpack_blob(TAG_DEL_WIRE_DATA, pp, &len, &p->del_wire_data) < 0 ||
	    unpack_int(TAG_SEED, pp, &len, &p->seed) < 0)
		return (-1);

	return (0);
}

static size_t
pack(uint8_t *ptr, size_t len, const struct param *p)
{
	const size_t max = len;

	if (pack_string(TAG_PIN, &ptr, &len, p->pin) < 0 ||
	    pack_string(TAG_RP_ID, &ptr, &len, p->rp_id) < 0 ||
	    pack_blob(TAG_CRED_ID, &ptr, &len, &p->cred_id) < 0 ||
	    pack_blob(TAG_META_WIRE_DATA, &ptr, &len, &p->meta_wire_data) < 0 ||
	    pack_blob(TAG_RP_WIRE_DATA, &ptr, &len, &p->rp_wire_data) < 0 ||
	    pack_blob(TAG_RK_WIRE_DATA, &ptr, &len, &p->rk_wire_data) < 0 ||
	    pack_blob(TAG_DEL_WIRE_DATA, &ptr, &len, &p->del_wire_data) < 0 ||
	    pack_int(TAG_SEED, &ptr, &len, p->seed) < 0)
		return (0);

	return (max - len);
}

static fido_dev_t *
prepare_dev()
{
	fido_dev_t	*dev;
	fido_dev_io_t	 io;

	io.open = dev_open;
	io.close = dev_close;
	io.read = dev_read;
	io.write = dev_write;

	if ((dev = fido_dev_new()) == NULL || fido_dev_set_io_functions(dev,
	    &io) != FIDO_OK || fido_dev_open(dev, "nodev") != FIDO_OK) {
		fido_dev_free(&dev);
		return (NULL);
	}

	return (dev);
}

static void
get_metadata(struct param *p)
{
	fido_dev_t *dev;
	fido_credman_metadata_t *metadata;
	uint64_t existing;
	uint64_t remaining;

	set_wire_data(p->meta_wire_data.body, p->meta_wire_data.len);

	if ((dev = prepare_dev()) == NULL) {
		return;
	}
	if ((metadata = fido_credman_metadata_new()) == NULL) {
		fido_dev_close(dev);
		fido_dev_free(&dev);
		return;
	}

	fido_credman_get_dev_metadata(dev, metadata, p->pin);

	existing = fido_credman_rk_existing(metadata);
	remaining = fido_credman_rk_remaining(metadata);
	consume(&existing, sizeof(existing));
	consume(&remaining, sizeof(remaining));

	fido_credman_metadata_free(&metadata);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
get_rp_list(struct param *p)
{
	fido_dev_t *dev;
	fido_credman_rp_t *rp;

	set_wire_data(p->rp_wire_data.body, p->rp_wire_data.len);

	if ((dev = prepare_dev()) == NULL) {
		return;
	}
	if ((rp = fido_credman_rp_new()) == NULL) {
		fido_dev_close(dev);
		fido_dev_free(&dev);
		return;
	}

	fido_credman_get_dev_rp(dev, rp, p->pin);

	/* +1 on purpose */
	for (size_t i = 0; i < fido_credman_rp_count(rp) + 1; i++) {
		consume(fido_credman_rp_id_hash_ptr(rp, i),
		    fido_credman_rp_id_hash_len(rp, i));
		consume(fido_credman_rp_id(rp, i),
		    xstrlen(fido_credman_rp_id(rp, i)));
		consume(fido_credman_rp_name(rp, i),
		    xstrlen(fido_credman_rp_name(rp, i)));
	}

	fido_credman_rp_free(&rp);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
get_rk_list(struct param *p)
{
	fido_dev_t *dev;
	fido_credman_rk_t *rk;
	const fido_cred_t *cred;
	int type;

	set_wire_data(p->rk_wire_data.body, p->rk_wire_data.len);

	if ((dev = prepare_dev()) == NULL) {
		return;
	}
	if ((rk = fido_credman_rk_new()) == NULL) {
		fido_dev_close(dev);
		fido_dev_free(&dev);
		return;
	}

	fido_credman_get_dev_rk(dev, p->rp_id, rk, p->pin);

	/* +1 on purpose */
	for (size_t i = 0; i < fido_credman_rk_count(rk) + 1; i++) {
		if ((cred = fido_credman_rk(rk, i)) == NULL) {
			assert(i >= fido_credman_rk_count(rk));
			continue;
		}
		type = fido_cred_type(cred);
		consume(&type, sizeof(type));
		consume(fido_cred_id_ptr(cred), fido_cred_id_len(cred));
		consume(fido_cred_pubkey_ptr(cred), fido_cred_pubkey_len(cred));
		consume(fido_cred_user_id_ptr(cred),
		    fido_cred_user_id_len(cred));
		consume(fido_cred_user_name(cred),
		    xstrlen(fido_cred_user_name(cred)));
		consume(fido_cred_display_name(cred),
		    xstrlen(fido_cred_display_name(cred)));
	}

	fido_credman_rk_free(&rk);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
del_rk(struct param *p)
{
	fido_dev_t *dev;

	set_wire_data(p->del_wire_data.body, p->del_wire_data.len);

	if ((dev = prepare_dev()) == NULL) {
		return;
	}

	fido_credman_del_dev_rk(dev, p->cred_id.body, p->cred_id.len, p->pin);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct param p;

	memset(&p, 0, sizeof(p));

	if (unpack(data, size, &p) < 0)
		return (0);

	srandom((unsigned int)p.seed);

	fido_init(0);

	get_metadata(&p);
	get_rp_list(&p);
	get_rk_list(&p);
	del_rk(&p);

	return (0);
}

static size_t
pack_dummy(uint8_t *ptr, size_t len)
{
	struct param	dummy;
	uint8_t		blob[32768];
	size_t		blob_len;

	memset(&dummy, 0, sizeof(dummy));

	strlcpy(dummy.pin, dummy_pin, sizeof(dummy.pin));
	strlcpy(dummy.rp_id, dummy_rp_id, sizeof(dummy.rp_id));

	dummy.meta_wire_data.len = sizeof(dummy_meta_wire_data);
	dummy.rp_wire_data.len = sizeof(dummy_rp_wire_data);
	dummy.rk_wire_data.len = sizeof(dummy_rk_wire_data);
	dummy.del_wire_data.len = sizeof(dummy_del_wire_data);
	dummy.cred_id.len = sizeof(dummy_cred_id);

	memcpy(&dummy.meta_wire_data.body, &dummy_meta_wire_data,
	    dummy.meta_wire_data.len);
	memcpy(&dummy.rp_wire_data.body, &dummy_rp_wire_data,
	    dummy.rp_wire_data.len);
	memcpy(&dummy.rk_wire_data.body, &dummy_rk_wire_data,
	    dummy.rk_wire_data.len);
	memcpy(&dummy.del_wire_data.body, &dummy_del_wire_data,
	    dummy.del_wire_data.len);
	memcpy(&dummy.cred_id.body, &dummy_cred_id, dummy.cred_id.len);

	blob_len = pack(blob, sizeof(blob), &dummy);
	assert(blob_len != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return (len);
	}

	memcpy(ptr, blob, blob_len);

	return (blob_len);
}

size_t
LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxsize,
    unsigned int seed) NO_MSAN
{
	struct param	p;
	uint8_t		blob[16384];
	size_t		blob_len;

	memset(&p, 0, sizeof(p));

	if (unpack(data, size, &p) < 0)
		return (pack_dummy(data, maxsize));

	p.seed = (int)seed;

	mutate_blob(&p.cred_id);
	mutate_blob(&p.meta_wire_data);
	mutate_blob(&p.rp_wire_data);
	mutate_blob(&p.rk_wire_data);
	mutate_blob(&p.del_wire_data);

	mutate_string(p.pin);
	mutate_string(p.rp_id);

	blob_len = pack(blob, sizeof(blob), &p);

	if (blob_len == 0 || blob_len > maxsize)
		return (0);

	memcpy(data, blob, blob_len);

	return (blob_len);
}
