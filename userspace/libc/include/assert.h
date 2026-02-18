#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
#define assert(condition) ((void)0) 
// TODO: Implement proper assertion failure
#endif

#endif /* _ASSERT_H */
