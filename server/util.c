#include "lols.h"
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

char **html_codes;
int *html_code_lengths;

typedef struct JSON_GC_TARGET json_gc_target;

struct JSON_GC_TARGET
{
  json_gc_target *next;
  char *str;
};

char *prep_for_json_gc( char *x );

json_gc_target *first_json_gc_target, *last_json_gc_target;

void log_string( char *txt )
{
  #ifdef LOLS_UNIX_CMDLINE
    printf( "%s\n", txt );
  #endif
}

void log_linenum( int linenum )
{
  #ifdef LOLS_UNIX_CMDLINE
    printf( "(Line %d)\n", linenum );
  #endif
}

void error_message( char *err )
{
  #ifdef LOLS_UNIX_CMDLINE
    log_string( err );
  #endif

  #ifdef LOLS_WINDOWS
    popup_error( err );
  #endif

  #ifdef LOLS_OSX
    /*
     * To do: error reporting for OS X
     */
  #endif
}

char *html_encode( char *str )
{
  char *buf;
  char *bptr, *sptr;
  char *rebuf;

  CREATE( buf, char, strlen(str)*6 + 1 );

  for ( bptr = buf, sptr = str; *sptr; sptr++ )
  {
    strcat( bptr, html_codes[(int)(*sptr)] );
    bptr += html_code_lengths[(int)(*sptr)];
  }

  CREATE( rebuf, char, strlen(buf)+1 );
  *rebuf = '\0';
  strcat( rebuf, buf );
  free(buf);

  return rebuf;
}

void init_html_codes( void )
{
  char i;

  CREATE( html_codes, char *, CHAR_MAX - CHAR_MIN + 1 );
  CREATE( html_code_lengths, int, CHAR_MAX - CHAR_MIN + 1 );

  html_codes = &html_codes[-CHAR_MIN];
  html_code_lengths = &html_code_lengths[-CHAR_MIN];

  for ( i = CHAR_MIN; ; i++ )
  {
    if ( i <= 0 )
    {
      html_codes[(int)i] = "";
      html_code_lengths[(int)i] = 0;
      continue;
    }
    switch(i)
    {
      default:
        CREATE( html_codes[(int)i], char, 2 );
        sprintf( html_codes[(int)i], "%c", i );
        html_code_lengths[(int)i] = 1;
        break;
      case '&':
        html_codes[(int)i] = "&amp;";
        html_code_lengths[(int)i] = strlen( "&amp;" );
        break;
      case '\"':
        html_codes[(int)i] = "&quot;";
        html_code_lengths[(int)i] = strlen( "&quot;" );
        break;
      case '\'':
        html_codes[(int)i] = "&#039;";
        html_code_lengths[(int)i] = strlen( "&#039;" );
        break;
      case '<':
        html_codes[(int)i] = "&lt;";
        html_code_lengths[(int)i] = strlen( "&lt;" );
        break;
      case '>':
        html_codes[(int)i] = "&gt;";
        html_code_lengths[(int)i] = strlen( "&gt;" );
        break;
    }
    if ( i == CHAR_MAX )
      break;
  }
}

char *lowercaserize( char *x )
{
  static char buf[MAX_STRING_LEN * 2];
  char *xptr = x;
  char *bptr = buf;

  for ( ; *xptr; xptr++ )
  {
    if ( *xptr >= 'A' && *xptr <= 'Z' )
      *bptr++ = *xptr + 'a' - 'A';
    else
      *bptr++ = *xptr;
  }

  *bptr = '\0';

  return buf;
}

char *get_url_shortform( char *iri )
{
  char *ptr;
  int end_index;

  if ( iri[0] != 'h' || iri[1] != 't' || iri[2] != 't' || iri[3] != 'p' )
    return NULL;

  end_index = strlen(iri)-1;

  ptr = &iri[end_index];

  do
  {
    if ( ptr <= iri )
    {
      ptr = &iri[end_index];

      do
      {
        if ( ptr <= iri )
          return NULL;
        if ( *ptr == '/' )
          return &ptr[1];
        ptr--;
      }
      while(1);
    }
    if ( *ptr == '#' )
      return &ptr[1];
    ptr--;
  }
  while(1);
}

/*
 * url_decode (and from_hex) courtesy of Fred Bulback (http://www.geekhideout.com/urlcode.shtml)
 */

/* Converts a hex character to its integer value */
char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

char *url_decode(char *str)
{
  char *pstr = str, *buf = malloc(strlen(str) + 1), *pbuf = buf;

  while (*pstr)
  {
    if (*pstr == '%')
    {
      if (pstr[1] && pstr[2])
      {
        *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
        pstr += 2;
      }
    }
    else
    if (*pstr == '+')
      *pbuf++ = ' ';
    else
      *pbuf++ = *pstr;

    pstr++;
  }

  *pbuf = '\0';
  return buf;
}

int is_number( const char *arg )
{
  int first = 1;

  if ( *arg == '\0' )
    return 0;

  for ( ; *arg != '\0'; arg++ )
  {
    if ( first && *arg == '-')
    {
      first = 0;
      continue;
    }

    if ( !isdigit(*arg) )
      return 0;

    first = 0;
  }

  return 1;
}

char *pretty_free( char *json )
{
  static char *pretty;
  char *err;

  if ( pretty )
    free( pretty );

  pretty = json_format( json, 2, &err );

  if ( !pretty )
  {
    pretty = json;
    log_string( "json_format failed during a call to pretty_free" );
    log_string( err );
  }
  else
    free( json );

  return pretty;
}

char *jsonf( int paircnt, ... )
{
  static char *fmt;
  char *result, *closing;
  va_list args;

  assert( paircnt <= FOR_EACH_MAX_ARGS );

  if ( !fmt )
  {
    fmt = strdup( "{%s:%s,%s:%s,%s:%s,%s:%s,%s:%s,%s:%s,%s:%s,%s:%s} " );

    /*
     * If FOR_EACH_MAX_ARGS is changed, then the number of
     * "%s:%s"'s above must be changed accordingly
     */
    assert( strlen(fmt) == 2+(FOR_EACH_MAX_ARGS * strlen("%s:%s,")) );
  }

  /*
   * Snip fmt to the appropriate size
   */
  closing = &fmt[ strlen("%s:%s,") * paircnt ];
  closing[0] = '}';
  closing[1] = '\0';

  /*
   * Delegate the hard work to vstrdupf
   */
  va_start( args, paircnt );
  result = vstrdupf( fmt, args );
  va_end( args );

  /*
   * Restore fmt to its original size
   */
  closing[0] = ',';
  closing[1] = '%';

  return prep_for_json_gc( result );
}

char *prep_for_json_gc( char *x )
{
  json_gc_target *t;

  if ( !x )
    return NULL;

  CREATE( t, json_gc_target, 1 );
  t->str = x;
  LINK( t, first_json_gc_target, last_json_gc_target, next );

  return x;
}

void json_gc( void )
{
  json_gc_target *t, *t_next;

  for ( t = first_json_gc_target; t; t = t_next )
  {
    t_next = t->next;

    free( t->str );
    free( t );
  }

  first_json_gc_target = NULL;
  last_json_gc_target = NULL;
}

char *jslist( json_array_printer *p, void **array )
{
  return jslist_r( p, array, NULL );
}

size_t voidlen( void **array )
{
  void **ptr;

  for ( ptr = array; *ptr; ptr++ )
    ;

  return ptr - array;
}

char *jslist_r( json_array_printer *p, void **array, void *param )
{
  char **jsons, **jsonsptr, *buf, *bptr;
  void **ptr;
  int len = strlen( "[]" ) - strlen( "," );
  int fFirst = 0;

  CREATE( jsons, char *, voidlen( array ) + 1 );

  for ( ptr = array, jsonsptr = jsons; *ptr; ptr++ )
  {
    *jsonsptr = (*p)( *ptr, param );
    len += strlen( *jsonsptr ) + strlen(",");
    prep_for_json_gc( *jsonsptr );
    jsonsptr++;
  }

  CREATE( buf, char, len + 1 );
  bptr = buf;
  *bptr++ = '[';

  for ( ptr = array, jsonsptr = jsons; *ptr; ptr++ )
  {
    if ( fFirst )
      *bptr++ = ',';
    else
      fFirst = 1;

    sprintf( bptr, "%s", *jsonsptr++ );
    bptr = &bptr[strlen(bptr)];
  }

  *bptr++ = ']';
  *bptr = '\0';

  free( jsons );

  return prep_for_json_gc( buf );
}
