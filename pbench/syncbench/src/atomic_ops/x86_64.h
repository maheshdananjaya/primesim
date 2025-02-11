/*
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996-1999 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1999-2003 by Hewlett-Packard Company. All rights reserved.
 *
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *
 * Some of the machine specific code was borrowed from our GC distribution.
 */

#include "./aligned_atomic_load_store.h"

/* Real X86 implementations appear                                      */
/* to enforce ordering between memory operations, EXCEPT that a later   */
/* read can pass earlier writes, presumably due to the visible          */
/* presence of store buffers.                                           */
/* We ignore the fact that the official specs                           */
/* seem to be much weaker (and arguably too weak to be usable).         */

#include "./ordered_except_wr.h"

#include "./test_and_set_t_is_char.h"

#include "./standard_ao_double_t.h"

AO_INLINE void
AO_nop_full(void)
{
  /* Note: "mfence" (SSE2) is supported on all x86_64/amd64 chips.      */
  __asm__ __volatile__("mfence" : : : "memory");
}

#define AO_HAVE_nop_full

/* As far as we can tell, the lfence and sfence instructions are not    */
/* currently needed or useful for cached memory accesses.               */

AO_INLINE AO_t
AO_fetch_and_add_full (volatile AO_t *p, AO_t incr)
{
  AO_t result;

  __asm__ __volatile__ ("lock; xaddq %0, %1" :
                        "=r" (result), "=m" (*p) : "0" (incr), "m" (*p)
                        : "memory");
  return result;
}

#define AO_HAVE_fetch_and_add_full

AO_INLINE unsigned char
AO_char_fetch_and_add_full (volatile unsigned char *p, unsigned char incr)
{
  unsigned char result;

  __asm__ __volatile__ ("lock; xaddb %0, %1" :
                        "=q" (result), "=m" (*p) : "0" (incr), "m" (*p)
                        : "memory");
  return result;
}

#define AO_HAVE_char_fetch_and_add_full

AO_INLINE unsigned short
AO_short_fetch_and_add_full (volatile unsigned short *p, unsigned short incr)
{
  unsigned short result;

  __asm__ __volatile__ ("lock; xaddw %0, %1" :
                        "=r" (result), "=m" (*p) : "0" (incr), "m" (*p)
                        : "memory");
  return result;
}

#define AO_HAVE_short_fetch_and_add_full

AO_INLINE unsigned int
AO_int_fetch_and_add_full (volatile unsigned int *p, unsigned int incr)
{
  unsigned int result;

  __asm__ __volatile__ ("lock; xaddl %0, %1" :
                        "=r" (result), "=m" (*p) : "0" (incr), "m" (*p)
                        : "memory");
  return result;
}

#define AO_HAVE_int_fetch_and_add_full

AO_INLINE void
AO_or_full (volatile AO_t *p, AO_t incr)
{
  __asm__ __volatile__ ("lock; orq %1, %0" :
                        "=m" (*p) : "r" (incr), "m" (*p) : "memory");
}

#define AO_HAVE_or_full

AO_INLINE AO_TS_VAL_t
AO_test_and_set_full(volatile AO_TS_t *addr)
{
  unsigned char oldval;
  /* Note: the "xchg" instruction does not need a "lock" prefix */
  __asm__ __volatile__("xchgb %0, %1"
                : "=q"(oldval), "=m"(*addr)
                : "0"(0xff), "m"(*addr) : "memory");
  return (AO_TS_VAL_t)oldval;
}

#define AO_HAVE_test_and_set_full

/* Returns nonzero if the comparison succeeded. */
AO_INLINE int
AO_compare_and_swap_full(volatile AO_t *addr, AO_t old, AO_t new_val)
{  
  //printf("atomic\n");
# ifdef AO_USE_SYNC_CAS_BUILTIN
  __asm__ __volatile__("sfence" : : : "memory");
  __asm__ __volatile__("lfence" : : : "memory");
    return (int)__sync_bool_compare_and_swap(addr, old, new_val);
   // __asm__ __volatile__("sfence" : : : "memory");
    //__asm__ __volatile__("mfence" : : : "memory");
# else
    char result;
    __asm__ __volatile__("sfence" : : : "memory");
    __asm__ __volatile__("lock; cmpxchgq %3, %0; setz %1"
                         : "=m" (*addr), "=a" (result)
                         : "m" (*addr), "r" (new_val), "a" (old) : "memory");
    //__asm__ __volatile__("sfence" : : : "memory");
    return (int) result;
# endif
}


//Acquire RMW Acquire
AO_INLINE int
AO_compare_and_swap_acq(volatile AO_t *addr, AO_t old, AO_t new_val)
{

# ifdef AO_USE_SYNC_CAS_BUILTIN
  __asm__ __volatile__("sfence" : : : "memory");
  __asm__ __volatile__("lfence" : : : "memory");
    return (int)__sync_bool_compare_and_swap(addr, old, new_val);
   // __asm__ __volatile__("sfence" : : : "memory");
    //__asm__ __volatile__("mfence" : : : "memory");
# else
     char result;
    __asm__ __volatile__("lfence" : : : "memory");
    __asm__ __volatile__("lock; cmpxchg %3, %0; setz %1"
                         : "=m" (*addr), "=a" (result)
                         : "m" (*addr), "r" (new_val), "a" (old) : "memory");
    // __asm__ __volatile__("mfence" : : : "memory");
     return (int) result;
#endif
}
//https://stackoverflow.com/questions/51598339/achieve-gcc-cas-function-for-version-4-1-2-and-earlier/51632252#51632252
//RMW Release
AO_INLINE int
AO_compare_and_swap_rel(volatile AO_t *addr, AO_t old, AO_t new_val)
{
 # ifdef AO_USE_SYNC_CAS_BUILTIN
  __asm__ __volatile__("sfence" : : : "memory");
  //__asm__ __volatile__("lfence" : : : "memory");
    return (int)__sync_bool_compare_and_swap(addr, old, new_val);
   // __asm__ __volatile__("sfence" : : : "memory");
    //__asm__ __volatile__("mfence" : : : "memory");
# else
    char result;
    __asm__ __volatile__("sfence" : : : "memory");
    __asm__ __volatile__("lock; cmpxchg %3, %0; setz %1"
                         : "=m" (*addr), "=a" (result)
                         : "m" (*addr), "r" (new_val), "a" (old) : "memory");
    //__asm__ __volatile__("mfence" : : : "memory");
     return (int) result;
#endif
}

#define AO_HAVE_compare_and_swap_full

#ifdef AO_CMPXCHG16B_AVAILABLE
/* NEC LE-IT: older AMD Opterons are missing this instruction.
 * On these machines SIGILL will be thrown.
 * Define AO_WEAK_DOUBLE_CAS_EMULATION to have an emulated
 * (lock based) version available */
/* HB: Changed this to not define either by default.  There are
 * enough machines and tool chains around on which cmpxchg16b
 * doesn't work.  And the emulation is unsafe by our usual rules.
 * Hoewever both are clearly useful in certain cases.
 */
AO_INLINE int
AO_compare_double_and_swap_double_full(volatile AO_double_t *addr,
                                       AO_t old_val1, AO_t old_val2,
                                       AO_t new_val1, AO_t new_val2)
{
  char result;
  __asm__ __volatile__("lock; cmpxchg16b %0; setz %1"
                       : "=m"(*addr), "=a"(result)
                       : "m"(*addr), "d" (old_val2), "a" (old_val1),
                         "c" (new_val2), "b" (new_val1) : "memory");
  return (int) result;
}
#define AO_HAVE_compare_double_and_swap_double_full
#else
/* this one provides spinlock based emulation of CAS implemented in     */
/* atomic_ops.c.  We probably do not want to do this here, since it is  */
/* not atomic with respect to other kinds of updates of *addr.  On the  */
/* other hand, this may be a useful facility on occasion.               */
#ifdef AO_WEAK_DOUBLE_CAS_EMULATION
int AO_compare_double_and_swap_double_emulation(volatile AO_double_t *addr,
                                                AO_t old_val1, AO_t old_val2,
                                                AO_t new_val1, AO_t new_val2);

AO_INLINE int
AO_compare_double_and_swap_double_full(volatile AO_double_t *addr,
                                       AO_t old_val1, AO_t old_val2,
                                       AO_t new_val1, AO_t new_val2)
{
        return AO_compare_double_and_swap_double_emulation(addr,
                                                           old_val1, old_val2,
                                                           new_val1, new_val2);
}
#define AO_HAVE_compare_double_and_swap_double_full
#endif /* AO_WEAK_DOUBLE_CAS_EMULATION */
#endif /* AO_CMPXCHG16B_AVAILABLE */
