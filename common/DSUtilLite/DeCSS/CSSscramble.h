#pragma once

extern void CSSdisckey(unsigned char *dkey, unsigned char *pkey);
extern void CSStitlekey(unsigned char *tkey, unsigned char *dkey);
extern void CSSdescramble(unsigned char *sector, unsigned char *tkey);

extern unsigned char g_PlayerKeys[][6U];
extern int g_nPlayerKeys;
