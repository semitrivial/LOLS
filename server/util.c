#include "lols.h"
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

char **html_codes;
int *html_code_lengths;

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

void error_messagef( char *err, ... )
{
  va_list vargs;
  char *tmp;

  va_start( vargs, err );

  tmp = vstrdupf( err, vargs );

  va_end( vargs );

  error_message( tmp );

  free( tmp );
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

char *lowercaserize( const char *x )
{
  static char buf[MAX_STRING_LEN * 2];
  const char *xptr = x;
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

void lowercaserize_destructive( char *x )
{
  for ( ; *x; x++ )
  {
    if ( *x >= 'A' && *x <= 'Z' )
      *x += 'a' - 'A';
  }
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

size_t voidlen( void **x )
{
  void **ptr;

  for ( ptr = x; *ptr; ptr++ )
    ;

  return ptr - x;
}

char *label_to_iri_to_json( trie *label )
{
  return JS_ARRAY( trie_to_json, label->data );
}

char *ont_from_full( char *full )
{
  char *fptr;
  char *shrt;

  if ( !full )
    return NULL;

  shrt = get_url_shortform( full );

  if ( !shrt )
    return NULL;

  // Skip '#' or '/' and then go back once more
  for ( fptr = &shrt[-2]; fptr >= full; fptr-- )
    if ( *fptr == '/' )
      break;

  if ( fptr < full )
    return NULL;

  fptr++;

  if ( fptr >= &shrt[-1] )
    return NULL;

  shrt[-1] = '\0';

  lowercaserize_destructive( fptr );

  return fptr;
}
