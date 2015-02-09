#ifndef LOLS_MACRO_DOT_H_INCLUDE_GUARD
#define LOLS_MACRO_DOT_H_INCLUDE_GUARD

/*
 * Memory allocation macro
 */
#define CREATE(result, type, number)\
do\
{\
    if (!((result) = (type *) calloc ((number), sizeof(type))))\
    {\
        fprintf(stderr, "Malloc failure at %s:%d\n", __FILE__, __LINE__ );\
        abort();\
    }\
} while(0)

/*
 * Bit-twiddling macros
 */
#define SET_BIT( flags, bit )\
do\
  (flags) |= (bit);\
while(0)

#define IS_SET( flags, bit ) ( (flags) & (bit) )

#define REMOVE_BIT( flags, bit )\
do\
  (flags) &= ~(bit);\
while(0)


/*
 * Link object into singly-linked list
 */
#define LINK( link, first, last, next )\
do\
{\
  (link)->next = NULL;\
  if ( last )\
    (last)->next = (link);\
  else\
    (first) = (link);\
  (last) = (link);\
} while(0)

/*
 * Link object into doubly-linked list
 */
#define LINK2(link, first, last, next, prev)\
do\
{\
   if ( !(first) )\
   {\
      (first) = (link);\
      (last) = (link);\
   }\
   else\
      (last)->next = (link);\
   (link)->next = NULL;\
   if (first == link)\
      (link)->prev = NULL;\
   else\
      (link)->prev = (last);\
   (last) = (link);\
} while(0)

/*
 * Unlink object from doubly-linked list
 */
#define UNLINK2(link, first, last, next, prev)\
do\
{\
        if ( !(link)->prev )\
        {\
         (first) = (link)->next;\
           if ((first))\
              (first)->prev = NULL;\
        }\
        else\
        {\
         (link)->prev->next = (link)->next;\
        }\
        if ( !(link)->next )\
        {\
         (last) = (link)->prev;\
           if ((last))\
              (last)->next = NULL;\
        }\
        else\
        {\
         (link)->next->prev = (link)->prev;\
        }\
} while(0)

#ifdef LOLS_WINDOWS

  #define QUICK_GETC( ch, fp )\
  do\
  {\
    if ( feof(fp) )\
      ch = '\0';\
    else\
      ch = getc(fp);\
  }\
  while(0)

#else

  /*
   * Quickly read char from file
   */
  #define QUICK_GETC( ch, fp )\
  do\
  {\
    if ( read_ptr == read_end )\
    {\
      fread_len = fread( read_buf, sizeof(char), READ_BLOCK_SIZE, fp );\
      if ( fread_len < READ_BLOCK_SIZE )\
        read_buf[fread_len] = '\0';\
      read_ptr = read_buf;\
    }\
    ch = *read_ptr++;\
  }\
  while(0)

#endif

#define QUICK_GETLINE( buffer, bufferptr, c, fp )\
do\
{\
  for( bufferptr = buffer; ;)\
  {\
    QUICK_GETC( c, fp );\
    if ( !c )\
    {\
      if ( buffer == bufferptr )\
        bufferptr = NULL;\
      else\
        *bufferptr = '\0';\
      \
      break;\
    }\
    \
    if ( c == '\n' )\
    {\
      *bufferptr = '\0';\
      break;\
    }\
    \
    if ( c != '\r' )\
      *bufferptr++ = c;\
  }\
}\
while(0)

/*
 * Timing macros
 */
#define TIMING_VARS struct timespec timespec1, timespec2

#define BEGIN_TIMING \
do\
{\
  clock_gettime(CLOCK_MONOTONIC, &timespec1 );\
}\
while(0)

#define END_TIMING \
do\
{\
  clock_gettime(CLOCK_MONOTONIC, &timespec2 );\
}\
while(0)

//#define TIMING_RESULT ( (timespec2.tv_sec - timespec1.tv_sec) * 1e+6 + (double) (timespec2.tv_nsec - timespec1.tv_nsec) * 1e-3 )
#define TIMING_RESULT ( (timespec2.tv_sec - timespec1.tv_sec) + (double) (timespec2.tv_nsec - timespec1.tv_nsec) * 1e-9 )

/*
 * Char-twiddling macros
 */
#define LOWER(x) ( ( (x) >= 'A' && (x) <= 'Z' )? x - 'A' + 'a' : x )

/*
 * FOR_EACH macro thanks to Gregory Pakosz.
 *
 * FOR_EACH( what, ... ) applies what to all following arguments.
 * Unfortunately, the max number of arguments must be hard-coded,
 * due to limitations in the preprocessor.  For our purposes
 * here, eight arguments will suffice.
 */

#define FOR_EACH_MAX_ARGS 8

#define FOR_EACH_1(what, x, ...) what(x)
#define FOR_EACH_2(what, x, ...)\
  what(x)\
  FOR_EACH_1(what,  __VA_ARGS__)
#define FOR_EACH_3(what, x, ...)\
  what(x)\
  FOR_EACH_2(what, __VA_ARGS__)
#define FOR_EACH_4(what, x, ...)\
  what(x)\
  FOR_EACH_3(what,  __VA_ARGS__)
#define FOR_EACH_5(what, x, ...)\
  what(x)\
 FOR_EACH_4(what,  __VA_ARGS__)
#define FOR_EACH_6(what, x, ...)\
  what(x)\
  FOR_EACH_5(what,  __VA_ARGS__)
#define FOR_EACH_7(what, x, ...)\
  what(x)\
  FOR_EACH_6(what,  __VA_ARGS__)
#define FOR_EACH_8(what, x, ...)\
  what(x)\
  FOR_EACH_7(what,  __VA_ARGS__)

#define FOR_EACH_NARG(...) FOR_EACH_NARG_(__VA_ARGS__, FOR_EACH_RSEQ_N())
#define FOR_EACH_NARG_(...) FOR_EACH_ARG_N(__VA_ARGS__)
#define FOR_EACH_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define FOR_EACH_RSEQ_N() 8, 7, 6, 5, 4, 3, 2, 1, 0

#define CONCATENATE(arg1, arg2)   CONCATENATE1(arg1, arg2)
#define CONCATENATE1(arg1, arg2)  CONCATENATE2(arg1, arg2)
#define CONCATENATE2(arg1, arg2)  arg1##arg2

#define STRINGIZE(arg)  STRINGIZE1(arg)
#define STRINGIZE1(arg) STRINGIZE2(arg)
#define STRINGIZE2(arg) #arg

#define FOR_EACH_(N, what, x, ...) CONCATENATE(FOR_EACH_, N)(what, x, __VA_ARGS__)
#define FOR_EACH(what, x, ...) FOR_EACH_(FOR_EACH_NARG(x, __VA_ARGS__), what, x, __VA_ARGS__)

/*
 * Magical JSON macros (using the FOR_EACH technology above)
 *
 * The novel discovery is the MAGIC_SPLIT macro which turns
 * a single argument, (key: val), into two arguments: (key), (val).
 * This is accomplished by hijacking the ternary operator.
 *
 * Example:
 * The expression
 *   JSON( "id": "1", "name": get_name() )
 * expands to
 *   jsonf( 2, "id", "1", "name", get_name() )
 * or rather, to be more specific, to
 *   jsonf( 0+1+1, "id", "1", "name", get_name() )
 */

#define MAGIC_SPLIT( x ) , 1 ? x, 0 ? x
#define ADD_ONE( x ) + 1

#define JSON(...)\
  jsonf\
  (\
    0 FOR_EACH( ADD_ONE, __VA_ARGS__ )\
    FOR_EACH( MAGIC_SPLIT, __VA_ARGS__ )\
  )

#endif // LOLS_MACRO_DOT_H_INCLUDE_GUARD
