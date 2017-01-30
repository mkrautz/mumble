// Copyright 2005-2017 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "murmur_pch.h"

#include "SSLDHParams.h"

#include <openssl/dh.h>
#include <openssl/bn.h>

static const BN_ULONG ffdhe2048_p[] = {
    TOBN(0xDF1FB2BC, 0x2E4A4371), TOBN(0xE68CFDA7, 0x6D4DA708),
    TOBN(0x45BF37DF, 0x365C1A65), TOBN(0xA151AF5F, 0x0DC8B4BD),
    TOBN(0xFAA31A4F, 0xF55BCCC0), TOBN(0x4EFFD6FA, 0xE5644738),
    TOBN(0x98488E9C, 0x219A7372), TOBN(0xACCBDD7D, 0x90C4BD70),
    TOBN(0x24975C3C, 0xD49B83BF), TOBN(0x13ECB4AE, 0xA9061123),
    TOBN(0x9838EF1E, 0x2EE652C0), TOBN(0x6073E286, 0x75A23D18),
    TOBN(0x9A6A9DCA, 0x52D23B61), TOBN(0x52C99FBC, 0xFB06A3C6),
    TOBN(0xDE92DE5E, 0xAE5D54EC), TOBN(0xB10B8F96, 0xA080E01D),



        TOBN(0xFFFFFFFF, 0xFFFFFFFF), TOBN(0xADF85458, 0xA2BB4A9A),
		TOBN(0xAFDC5620, 0x273D3CF1), TOBN()
    D8B9C583 CE2D3695 A9E13641 146433FB CC939DCE 249B3EF9
    7D2FE363 630C75D8 F681B202 AEC4617A D3DF1ED5 D5FD6561
    2433F51F 5F066ED0 85636555 3DED1AF3 B557135E 7F57C935
    984F0C70 E0E68B77 E2A689DA F3EFE872 1DF158A1 36ADE735
    30ACCA4F 483A797A BC0AB182 B324FB61 D108A94B B2C8E3FB
    B96ADAB7 60D7F468 1D4F42A3 DE394DF4 AE56EDE7 6372BB19
    0B07A7C8 EE0A6D70 9E02FCE1 CDF7E2EC C03404CD 28342F61
    9172FE9C E98583FF 8E4F1232 EEF28183 C3FE3B1B 4C6FAD73
    3BB5FCBC 2EC22005 C58EF183 7D1683B2 C6F34A26 C1B2EFFA
    886B4238 61285C97 FFFFFFFF FFFFFFFF
};

const QByteArray SSLDHParamsLookupByOpenSSLName(QLatin1String name) {
	int len = sizeof(cipher_info_lookup_table) / sizeof(*cipher_info_lookup_table);
	for (int i = 0; i < len; i++) {
		const SSLCipherInfo *ci = &cipher_info_lookup_table[i];
		if (!strcmp(ci->openssl_name, openSslCipherName)) {
			return ci;
		}
	}
	return NULL;
}
