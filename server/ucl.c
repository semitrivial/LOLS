#include "lols.h"

ucl_syntax *parse_ucl_syntax( char *ucl, char **err )
{
  char *end = &ucl[strlen(ucl)];
  char *ptr;
  ucl_syntax *s;
  int lparens, token_ready;
  trie *t;

  while ( *ucl == ' ' )
    ucl++;

  while ( *end == ' ' && end > ucl )
    end--;

  if ( ucl == end )
  {
    *err = strdup( "Empty expression or sub-expression" );
    return NULL;
  }

  end[1] = '\0';

  if ( *ucl == '(' && *end == ')' )
  {
    *end = '\0';

    CREATE( s, ucl_syntax, 1 );
    s->type = UCL_SYNTAX_PAREN;
    s->sub1 = parse_ucl_syntax( &ucl[1], err );

    if ( !s->sub1 )
    {
      free( s );
      return NULL;
    }

    CREATE( s->toString, char, strlen( s->sub1->toString ) + strlen( "()" ) + 1 );

    sprintf( s->toString, "(%s)", s->sub1->toString );

    return s;
  }

  if ( str_begins( ucl, "not" ) )
  {
    CREATE( s, ucl_syntax, 1 );
    s->type = UCL_SYNTAX_NOT;
    s->sub1 = parse_ucl_syntax( &ucl[1], err );

    if ( !s->sub1 )
    {
      free( s );
      return NULL;
    }

    CREATE( s->toString, char, strlen( s->sub1->toString ) + strlen( "not " ) + 1 );

    sprintf( s->toString, "not %s", s->sub1->toString );
  }

  for ( ptr = ucl, lparens = token_ready = 0; *ptr; ptr++ )
  {
    switch( *ptr )
    {
      default:
        token_ready = 0;
        break;

      case '(':
        lparens++;
        token_ready = 0;
        break;

      case ')':
        if ( !lparens )
        {
          *err = strdup( "Mismatched right parenthesis" );
          return NULL;
        }

        lparens--;
        token_ready = 1;
        break;

      case ' ':
        token_ready = 1;
        break;

      case 's':
      case 'a':
      case 'o':
        if ( !token_ready )
          break;

        token_ready = 0;

        if ( ptr == ucl )
          break;

        if ( *ptr == 's' && str_begins( ptr, "some" ) )
        {
          CREATE( s, ucl_syntax, 1 );
          s->type = UCL_SYNTAX_SOME;
          s->sub1 = parse_ucl_syntax( &ucl[strlen("some")], err );

          if ( !s->sub1 )
          {
            free( s );
            return NULL;
          }

          s->reln = read_some_relation( ucl, ptr );

          CREATE( s->toString, char, strlen( s->reln ) + strlen( " some " ) + strlen( s->sub1->toString ) + 1 );
          sprintf( s->toString, "%s some %s", s->reln, s->sub1->toString );
        }

        if ( ( *ptr == 'a' && str_begins( ptr, "and" ) )
        ||   ( *ptr == 'o' && str_begins( ptr, "or" ) ) )
        {
          if ( ptr == ucl )
            return NULL;

          CREATE( s, ucl_syntax, 1 );
          s->type = (*ptr == 'a') ? UCL_SYNTAX_AND : UCL_SYNTAX_OR;

          *ptr = '\0';
          s->sub1 = parse_ucl_syntax( ucl, err );

          if ( !s->sub1 )
          {
            free( s );
            return NULL;
          }

          s->sub2 = parse_ucl_syntax( &ucl[(*ptr == 'a') ? strlen("and") : strlen("or") ], err );

          if ( !s->sub2 )
          {
            kill_ucl_syntax( s->sub1 );
            free( s );
            return NULL;
          }

          CREATE( s->toString, char, strlen( s->sub1->toString ) + strlen( " and " ) + strlen( s->sub2->toString ) + 1 );

          sprintf( s->toString, "%s %s %s", s->sub1->toString, (*ptr=='a') ? "and" : "or", s->sub2->toString );

          return s;
        }
        break;
    }
  }

  t = trie_search( ucl, label_to_iris_lowercase );

  if ( !t )
  {
    trie_search( ucl, iri_to_labels );

    if ( !t )
    {
      CREATE( *err, char, strlen( ucl ) + strlen( "Unrecognized IRI: " ) );
      sprintf( *err, "Unrecognized IRI: %s", ucl );
      return NULL;
    }
  }

  CREATE( s, ucl_syntax, 1 );
  s->type = UCL_SYNTAX_BASE;
  s->iri = t;

  ptr = trie_to_static( s->iri );

  CREATE( s->toString, char, strlen(ptr) + 1 );

  sprintf( s->toString, "%s", ptr );

  return s;
}

int str_begins( char *full, char *init )
{
  char tmp;
  int retval;
  int full_len = strlen( full );
  int init_len = strlen( init );

  if ( full_len < init_len )
    return 0;

  tmp = full[init_len];

  full[init_len] = '\0';

  retval = !strcmp( full, init )
        && ( tmp == ' ' || tmp == '(' || tmp == '\0' );

  full[init_len] = tmp;

  return retval;
}

char *read_some_relation( char *left, char *right )
{
  if ( left == right )
    return "";

  while ( *left == ' ' )
  {
    left++;

    if ( left == right )
      return "";
  }

  do
  {
    right--;

    if ( left == right )
      return "";
  }
  while ( *right == ' ' );

  if ( *left == '(' && *right == ')' )
    return read_some_relation( &left[1], right );

  *right = '\0';

  return left;
}
