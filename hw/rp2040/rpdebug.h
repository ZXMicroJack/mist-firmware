#ifndef _DEBUG_H
#define _DEBUG_H

#if 0
#ifdef DEBUG
void hexdump1(uint8_t *buf, int len);
#ifdef PIODEBUG
void debuginit();
int dbgprintf(const char *fmt, ...);
#define debug(a) dbgprintf a
#else
#define debuginit()
#define debug(a) printf a
#endif
#else
#define debuginit()
#define debug(a)
#undef hexdump1
#define hexdump1(a,b)
#endif
#endif

#ifdef DEBUG
#define debug(a) printf a
#else
#define debug(a)
#endif


#endif
