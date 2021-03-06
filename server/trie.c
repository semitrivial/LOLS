#include "lols.h"

trie *iri_to_labels;
trie *label_to_iris;
trie *label_to_iris_lowercase;
trie *predicates_full;
trie *predicates_short;

void populate_void_buffer( void ***ptr, trie *t );

#ifdef LOLS_WINDOWS
char *strndup(const char *x, int len)
{
  char *buf;
  int x_len = strlen(x);

  if (len < x_len)
    x_len = len;

  CREATE( buf, char, len + 1 );

  buf[x_len] = '\0';
  return memcpy(buf, x, x_len);
}
#endif

trie *blank_trie( void )
{
  trie *t;

  CREATE( t, trie, 1 );
  t->parent = NULL;
  t->children = NULL;
  t->label = NULL;
  t->data = NULL;
  t->ont = NULL;

  return t;
}

trie *trie_strdup( char *buf, trie *base )
{
  char *bptr;
  trie *t;
  int cnt;

  bptr = buf;
  t = base;

  for(;;)
  {
    trie_strdup_main_loop:

    if ( !*bptr )
      return t;

    if ( t->children )
    {
      trie **child, *blank=NULL;

      for ( child = t->children, cnt=0; *child; child++ )
      {
        cnt++;

        if ( !*((*child)->label) )
        {
          blank = *child;
          continue;
        }

        if ( *((*child)->label) == *bptr )
        {
          /*
           * Found an edge with same initial char as bptr
           */
          char *bx, *lx;

          for ( bx = bptr, lx = ((*child)->label); ; )
          {
            if ( !*lx )
            {
              bptr = bx;
              t = *child;
              goto trie_strdup_main_loop;
            }

            if ( !*bx )
            {
              trie *inter = blank_trie(), *cx = *child;
              char *tmp;

              inter->parent = t;
              inter->label = strndup(cx->label, lx-(cx->label) );
              CREATE( inter->children, trie *, 2 );
              inter->children[0] = cx;
              inter->children[1] = NULL;

              tmp = cx->label;
              cx->label = strdup(lx);
              free( tmp );
              cx->parent = inter;
              *child = inter;

              return inter;
            }

            if ( *lx != *bx )
            {
              trie *inter = blank_trie(), *cx = *child, *cxx;
              char *oldlabel;

              inter->parent = t;
              CREATE( inter->children, trie *, 3 );
              inter->children[0] = cx;
              inter->children[1] = blank_trie();
              inter->children[2] = NULL;

              cxx = inter->children[1];
              cxx->parent = inter;
              cx->parent = inter;
              *child = inter;

              inter->label = strndup(cx->label, (lx-cx->label));

              oldlabel = cx->label;

              cx->label = strdup(lx);
              free(oldlabel);
              cxx->label = strdup(bx);

              return cxx;
            }

            lx++;
            bx++;
          }
        }
      }

      /*
       * No edges have the same first char as bptr.
       * If there was a blank edge, use that.
       */
      if ( blank )
      {
        free( blank->label );
        blank->label = strdup(bptr);
        return blank;
      }
      else
      {
        /*
         * If no edges have same first char as bptr,
         * and there are no blank edges, then create new edge.
         */
        trie **children, *cx;

        CREATE( children, trie *, cnt + 2 );
        if ( t->children )
          memcpy( children, t->children, sizeof(trie*) * cnt );
        children[cnt] = blank_trie();
        cx = children[cnt];
        cx->parent = t;
        cx->label = strdup( bptr );
        children[cnt+1] = NULL;

        free( t->children );
        t->children = children;

        return cx;
      }
    }
    else
    {
      /*
       * No outgoing edges:
       * Create one.
       */
      trie *cx;

      CREATE( t->children, trie *, 2 );
      t->children[0] = blank_trie();
      cx = t->children[0];
      cx->parent = t;
      cx->label = strdup(bptr);

      t->children[1] = NULL;

      return cx;
    }
  }
}

trie *trie_search( char *buf, trie *base )
{
  char *bptr;
  trie *t;

  bptr = buf;
  t = base;

  for(;;)
  {
    trie_search_main_loop:

    if ( !*bptr )
      return t;

    if ( t->children )
    {
      trie **child;

      for ( child = t->children; *child; child++ )
      {
        if ( *((*child)->label) == *bptr )
        {
          /*
           * Found an edge with same initial char as bptr
           */
          char *bx, *lx;

          for ( bx = bptr, lx = ((*child)->label); ; )
          {
            if ( !*lx )
            {
              bptr = bx;
              t = *child;
              goto trie_search_main_loop;
            }

            if ( !*bx )
              return NULL;

            if ( *lx != *bx )
              return NULL;

            lx++;
            bx++;
          }
        }
      }

      return NULL;
    }
    else
      return NULL;
  }
}

char *trie_to_json( trie *t )
{
  return str_to_json( t ? trie_to_static( t ) : NULL );
}

char *trie_to_static( trie *t )
{
  static char buf[MAX_STRING_LEN + 2], *bptr;
  trie *ancestor;
  char *label, *lptr;

  bptr = &buf[MAX_STRING_LEN+1];
  *bptr-- = '\0';

  ancestor = t;

  while ( ancestor->parent )
  {
    label = ancestor->label;

    lptr = &label[strlen(label)-1];
    do
    {
      if ( bptr <= buf )
      {
        log_string( "Error_tree: trie_to_static ran out of space!\n" );
        return buf;
      }
      *bptr-- = *lptr--;
    }
    while ( lptr >= label );

    ancestor = ancestor->parent;
  }

  return &bptr[1];
}

void trie_search_autocomplete( char *label_ch, trie **buf, trie *base, ont_name *ont )
{
  char *chptr = label_ch;
  trie *t = base;

  for (;;)
  {
    trie_search_autocomplete_loop:

    if ( !*chptr )
      break;

    if ( !t->children )
    {
      *buf = NULL;
      return;
    }
    else
    {
      trie **child;

      for ( child = t->children; *child; child++ )
      {
        if ( *((*child)->label) == *chptr )
        {
          char *chx, *lx;

          for ( chx = chptr, lx = ((*child)->label); ; )
          {
            if ( !*lx )
            {
              chptr = chx;
              t = *child;
              goto trie_search_autocomplete_loop;
            }

            if ( !*chx )
            {
              t = *child;
              goto trie_search_autocomplete_escape;
            }

            if ( *lx != *chx )
            {
              *buf = NULL;
              return;
            }

            lx++;
            chx++;
          }
        }
      }
      *buf = NULL;
      return;
    }
  }

  trie_search_autocomplete_escape:
  {
    int finds = 0;
    trie **bptr = buf, **children;
    trie_wrapper *head=NULL, *tail=NULL, *wrap, *wptr, *wptr_next;

    if ( t->data )
    {
      if ( base == label_to_iris )
        maybe_add_autocomplete_result( &bptr, t, &finds, ont );
      else
        maybe_add_autocomplete_result( &bptr, t->data[0]->data[0], &finds, ont );
    }

    if ( t->children )
    {
      for ( children = t->children; *children; children++ )
      {
        CREATE( wrap, trie_wrapper, 1 );
        wrap->t = *children;
        LINK2( wrap, head, tail, next, prev );
      }
    }

    for ( wptr = head; wptr; wptr = wptr->next )
    {
      if ( wptr->t->data )
      {
        if ( base == label_to_iris )
          maybe_add_autocomplete_result( &bptr, wptr->t, &finds, ont );
        else
          maybe_add_autocomplete_result( &bptr, wptr->t->data[0]->data[0], &finds, ont );

        if ( finds >= MAX_AUTOCOMPLETE_RESULTS_PRESORT )
          break;
      }

      if ( wptr->t->children )
      {
        for ( children = wptr->t->children; *children; children++ )
        {
          CREATE( wrap, trie_wrapper, 1 );
          wrap->t = *children;
          LINK2( wrap, head, tail, next, prev );
        }
      }
    }

    *bptr = NULL;
    for ( wptr = head; wptr; wptr = wptr_next )
    {
      wptr_next = wptr->next;
      free( wptr );
    }

    qsort(buf, finds, sizeof(trie*), cmp_trie_data);

    if ( finds > MAX_AUTOCOMPLETE_RESULTS_POSTSORT )
      buf[MAX_AUTOCOMPLETE_RESULTS_POSTSORT] = NULL;
  }

  return;
}

void maybe_add_autocomplete_result( trie ***bptr, trie *t, int *finds, ont_name *n )
{
  trie *iri_tr;
  char *ont;

  if ( !t->data )
    return;

  if ( !n )
  {
    maybe_add_autocomplete_result_add:

    **bptr = t;
    (*bptr)++;
    (*finds)++;
    return;
  }

  iri_tr = t->data[0];
  ont = get_ont_by_iri( trie_to_static( iri_tr ) );

  if ( ont && !strcmp( ont, n->friendly ) )
    goto maybe_add_autocomplete_result_add;
}

int cmp_trie_data (const void * a, const void * b)
{
  int len1 = strlen( trie_to_static( *((trie**)a) ) );
  int len2 = strlen( trie_to_static( *((trie**)b) ) );

  return len1-len2;
}

void kill_ucl_syntax( ucl_syntax *s )
{
  switch( s->type )
  {
    default:
      break;

    case UCL_SYNTAX_AND:
    case UCL_SYNTAX_OR:
      kill_ucl_syntax( s->sub2 );

    case UCL_SYNTAX_PAREN:
    case UCL_SYNTAX_SOME:
    case UCL_SYNTAX_NOT:
      kill_ucl_syntax( s->sub1 );
  }

  free( s->toString );
  free( s );
}

void **datas_to_array( trie *t )
{
  int cnt = count_nontrivial_members( t );
  void **buf, **bptr;

  CREATE( buf, void *, cnt + 1 );
  bptr = buf;

  populate_void_buffer( &bptr, t );

  *bptr = NULL;

  return buf;
}

void populate_void_buffer( void ***ptr, trie *t )
{
  if ( t->data )
  {
    **ptr = (void *)t->data;
    (*ptr)++;
  }

  if ( t->children )
  {
    trie **child;

    for ( child = t->children; *child; child++ )
      populate_void_buffer( ptr, *child );
  }
}

int count_nontrivial_members( trie *t )
{
  int cnt;

  if ( t->data )
    cnt = 1;
  else
    cnt = 0;

  if ( t->children )
  {
    trie **child;

    for ( child = t->children; *child; child++ )
      cnt += count_nontrivial_members( *child );
  }

  return cnt;
}
