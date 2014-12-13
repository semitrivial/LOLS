#include "lols.h"

ucl_syntax *parse_ucl_syntax( char *ucl, char **err )
{
  char *end = &ucl[strlen(ucl)-1], *ptr, *shorturl, *unshort;
  ucl_syntax *s;
  int lparens, token_ready;
  trie *t;

  if ( end < ucl )
  {
    *err = strdup( "Empty expression or sub-expression" );
    return NULL;
  }

  while ( *ucl == ' ' )
    ucl++;

  while ( *end == ' ' && end > ucl )
    end--;

  if ( ucl >= end )
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
    s->sub1 = parse_ucl_syntax( &ucl[strlen("not")], err );

    if ( !s->sub1 )
    {
      free( s );
      return NULL;
    }

    CREATE( s->toString, char, strlen( s->sub1->toString ) + strlen( "not " ) + 1 );

    sprintf( s->toString, "not %s", s->sub1->toString );

    return s;
  }

  for ( ptr = ucl, lparens = token_ready = 0; *ptr; ptr++ )
  {
    if ( lparens && *ptr != '(' && *ptr != ')' )
      continue;

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
          if ( ptr == ucl )
          {
            *err = strdup( "Encountered 'some' with no relation" );
            return NULL;
          }

          CREATE( s, ucl_syntax, 1 );
          s->type = UCL_SYNTAX_SOME;
          s->sub1 = parse_ucl_syntax( &ptr[strlen("some")], err );

          if ( !s->sub1 )
          {
            free( s );
            return NULL;
          }

          s->reln = read_some_relation( ucl, &ptr[-1] );

          CREATE( s->toString, char, strlen( s->reln ) + strlen( " some " ) + strlen( s->sub1->toString ) + 1 );
          sprintf( s->toString, "%s some %s", s->reln, s->sub1->toString );

          return s;
        }

        if ( ( *ptr == 'a' && str_begins( ptr, "and" ) )
        ||   ( *ptr == 'o' && str_begins( ptr, "or" ) ) )
        {
          if ( ptr == ucl )
            return NULL;

          CREATE( s, ucl_syntax, 1 );
          s->type = (*ptr == 'a') ? UCL_SYNTAX_AND : UCL_SYNTAX_OR;

          if ( *ptr == 'a' )
            s->sub2 = parse_ucl_syntax( &ptr[strlen("and")], err );
          else
            s->sub2 = parse_ucl_syntax( &ptr[strlen("or")], err );

          if ( !s->sub2 )
          {
            free( s );
            return NULL;
          }

          *ptr = '\0';
          s->sub1 = parse_ucl_syntax( ucl, err );

          if ( !s->sub1 )
          {
            kill_ucl_syntax( s->sub2 );
            free( s );
            return NULL;
          }

          CREATE( s->toString, char, strlen( s->sub1->toString ) + strlen( " and " ) + strlen( s->sub2->toString ) + 1 );

          sprintf( s->toString, "%s %s %s", s->sub1->toString, (s->type == UCL_SYNTAX_AND) ? "and" : "or", s->sub2->toString );

          return s;
        }
        break;
    }
  }

  t = trie_search( ucl, label_to_iris_lowercase );

  if ( t && t->data && *t->data )
  {
    CREATE( s, ucl_syntax, 1 );
    unshort = trie_to_static( *t->data );
    shorturl = get_url_shortform( unshort );
    s->toString = strdup( shorturl ? shorturl : unshort );
  }
  else
  {
    t = trie_search( ucl, iri_to_labels );

    if ( t )
    {
      CREATE( s, ucl_syntax, 1 );
      unshort  = trie_to_static( t );
      shorturl = get_url_shortform( unshort );
      s->toString = strdup( shorturl ? shorturl : unshort );
    }
    else
    {
      CREATE( *err, char, strlen( ucl ) + strlen( "Unrecognized IRI: " ) );
      sprintf( *err, "Unrecognized IRI: %s", ucl );
      return NULL;
    }
  }

  s->type = UCL_SYNTAX_BASE;
  s->iri = t;

  return s;
}

int str_begins( char *full, char *init )
{
  char *fptr = full, *iptr = init;

  for (;;)
  {
    if ( !*fptr && !*iptr )
      return 1;

    if ( !*fptr )
      return 0;

    if ( !*iptr )
      return ( !*fptr || (*fptr==' ') || (*fptr=='(') );

    if ( LOWER( *fptr ) != LOWER( *iptr ) )
      return 0;

    fptr++;
    iptr++;
  }
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

  while ( right > left && right[-1] == ' ' )
  {
    right--;

    if ( left == right )
      return "";
  }

  if ( *left == '(' && *right == ')' )
    return read_some_relation( &left[1], right-1 );

  *right = '\0';

  return left;
}
