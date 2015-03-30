#include "lols.h"
#include "nt_parse.h"

ont_name *first_ont_name;
ont_name *last_ont_name;
trie_wrapper *first_ambig_label;
trie_wrapper *last_ambig_label;

void init_lols(void)
{
  iri_to_labels = blank_trie();
  label_to_iris = blank_trie();
  label_to_iris_lowercase = blank_trie();
  predicates_full = blank_trie();
  predicates_short = blank_trie();

  init_html_codes();

  return;
}

void got_triple( char *subj, char *pred, char *obj )
{
  if ( !strcmp( pred, "<http://www.w3.org/2000/01/rdf-schema#label>" )
  ||   !strcmp( pred, "<rdfs:label>" ) )
  {
    if ( *obj == '"' && *subj == '<' )
    {
      obj[strlen(obj)-1] = '\0';
      subj[strlen(subj)-1] = '\0';

      add_lols_entry( &subj[1], &obj[1] );
    }
  }

  if ( !strcmp( pred, "<http://open-physiology.org/ont#predicate_label>" ) )
  {
    if ( *subj == '<' && *obj == '\"' )
    {
      subj[strlen(subj)-1] = '\0';
      obj[strlen(obj)-1] = '\0';
      add_lols_predicate( &subj[1], &obj[1] );
    }
  }

  free( subj );
  free( pred );
  free( obj );
}

void parse_lols_file(FILE *fp)
{
  char *err = NULL;

  if ( !parse_ntriples( fp, &err, MAX_IRI_LEN, got_triple ) )
  {
    char *buf = malloc(strlen(err) + 1024);

    sprintf( buf, "Failed to parse the triples-file:\n%s\n", err );

    error_message( buf );
    abort();
  }

  if ( unambig_mode && !resolve_ambig_labels() )
  {
    error_message( "Error: There were ambiguous labels that could not be resolved." );
    error_message( "This causes LOLS to halt because you are running LOLS in unambiguous mode." );
    error_message( "In order to help LOLS resolve ambiguous labels, please specify relative priorities of ontologies in a LOLS config file.\n" );

    display_unresolved_ambig_labels();
    EXIT();
  }
}

void add_to_data_worker( trie ***dest, trie *datum, int avoid_dupes )
{
  trie **data;
  int cnt;

  if ( !*dest )
  {
    CREATE( *dest, trie *, 2 );
    (*dest)[0] = datum;
    (*dest)[1] = NULL;
  }
  else
  {
    if ( avoid_dupes )
    {
      for ( data = *dest; *data; data++ )
      {
        if ( *data == datum )
          return;
      }
    }
    else
    {
      for ( data = *dest; *data; data++ )
        ;
    }
    cnt = data - *dest;

    CREATE( data, trie *, cnt + 2 );
    memcpy( data, *dest, cnt * sizeof(trie*) );
    data[cnt] = datum;
    free( *dest );
    *dest = data;
  }
}

void add_to_data( trie ***dest, trie *datum )
{
  add_to_data_worker( dest, datum, 0 );
}

void add_to_data_avoid_dupes( trie ***dest, trie *datum )
{
  add_to_data_worker( dest, datum, 1 );
}

void add_lols_predicate( char *iri_ch, char *label_ch )
{
  trie *full, *label;
  char *ont;

  if ( !*iri_ch )
    return;

  full = trie_strdup( iri_ch, predicates_full );
  label = trie_strdup( label_ch, predicates_short );

  add_to_data_avoid_dupes( &label->data, full );
  add_to_data( &full->data, label );

  if ( unambig_mode && label->data[1] && !label->data[2] )
    mark_ambiguous_label( label );

  lowercaserize_destructive( iri_ch );
  ont = get_ont_by_iri( iri_ch );

  if ( ont )
    full->ont = ont;
  else
  {
    if ( unambig_mode )
    {
      error_message( "Error: Encountered an IRI for which an ontology could not be deduced." );
      error_messagef( "[%s]", iri_ch );
      error_message( "This causes LOLS to halt because LOLS is being run in nonambiguous mode" );

      EXIT();
    }
  }
}

void add_lols_entry( char *iri_ch, char *label_ch )
{
  char *iri_shortform_ch, *label_lowercase_ch;
  trie *iri = trie_strdup(iri_ch, iri_to_labels);
  trie *label = trie_strdup(label_ch, label_to_iris);
  trie *label_lowercase;

  add_to_data( &iri->data, label );
  add_to_data( &label->data, iri );

  if ( (iri_shortform_ch = get_url_shortform(iri_ch)) != NULL )
  {
    trie *iri_shortform = trie_strdup( iri_shortform_ch, iri_to_labels );

    if ( unambig_mode )
    {
      char *ont = get_ont_by_iri( iri_ch );

      if ( !ont )
      {
        error_message( "Encountered an IRI with no ontology name associated with it:" );
        error_message( iri_ch );
        error_message( "(This causes LOLS to stop because LOLS was launched in unambiguous mode)" );

        EXIT();
      }

      if ( label->data[1] && !label->data[2] )
        mark_ambiguous_label( label );

      iri->ont = ont;
    }

    add_to_data( &iri_shortform->data, label );
  }
  else if ( unambig_mode )
  {
    error_message( "There was an IRI with no shotform:" );
    error_messagef( "[%s]", iri_ch );
    error_message( "(This causes LOLS to stop because LOLS was launched in unambiguous mode)" );

    EXIT();
  }

  label_lowercase_ch = lowercaserize(label_ch);
  label_lowercase = trie_strdup( label_lowercase_ch, label_to_iris_lowercase );
  add_to_data( &label_lowercase->data, iri );

  return;
}

trie **get_labels_by_iri( char *iri_ch )
{
  trie *iri = trie_search( iri_ch, iri_to_labels );

  if ( !iri )
    return NULL;

  return iri->data;
}

trie **get_iris_by_label( char *label_ch )
{
  trie *label = trie_search( label_ch, label_to_iris );

  if ( !label )
    return NULL;

  return label->data;
}

trie **get_iris_by_label_case_insensitive( char *label_ch )
{
  char *lowercase = lowercaserize( label_ch );
  trie *label = trie_search( lowercase, label_to_iris_lowercase );

  if ( !label )
    return NULL;

  return label->data;
}

trie **get_autocomplete_labels( char *label_ch, int case_insens )
{
  static trie **buf;

  if ( case_insens )
    label_ch = lowercaserize( label_ch );

  if ( !buf )
    CREATE( buf, trie *, MAX_AUTOCOMPLETE_RESULTS_PRESORT + 1 );

  trie_search_autocomplete( label_ch, buf, case_insens ? label_to_iris_lowercase : label_to_iris );

  return buf;
}

ont_name *ont_name_by_str( char *str )
{
  ont_name *n;

  for ( n = first_ont_name; n; n = n->next )
    if ( !strcmp( n->friendly, str ) )
      return n;

  return NULL;
}

char *get_ont_by_iri( char *full )
{
  ont_name *o;

  for ( o = first_ont_name; o; o = o->next )
    if ( str_begins( full, o->namespace ) )
      return o->friendly;

  return NULL;
}

void mark_ambiguous_label( trie *label )
{
  trie_wrapper *w;

  CREATE( w, trie_wrapper, 1 );
  w->t = label;

  LINK2( w, first_ambig_label, last_ambig_label, next, prev );
}

/*
 * iri_to_json is inherently memory-leaky because it's (currently) only
 * intended to be used immediately prior to LOLS shutting down
 */
char *iri_to_json( trie *iri )
{
  return strdupf( "%s (%s)", trie_to_static( iri ), iri->ont );
}

void display_unresolved_ambig_labels( void )
{
  if ( first_ambig_label == last_ambig_label )
  {
    error_messagef( "The ambiguous label is: [%s]", trie_to_static(first_ambig_label->t) );
    error_messagef( "Its IRIs are:\n%s", json_format( JS_ARRAY( iri_to_json, first_ambig_label->t->data ), 1, NULL ) );
  }
  else
  {
    int i, upperlimit;
    trie_wrapper *w;

    if ( configs.unresolved_ambigs_full_details )
    {
      error_message( "The ambiguous labels are:" );
      upperlimit = 64;
    }
    else
    {
      error_message( "The first few ambiguous labels are:" );
      upperlimit = 5;
    }

    for ( i = 0, w = first_ambig_label; w && i < upperlimit; w = w->next, i++ )
    {
      error_messagef( "%s", trie_to_static( w->t ) );

      if ( configs.unresolved_ambigs_full_details )
        error_messagef( "...which has IRIs:\n%s\n---------", json_format( JS_ARRAY( iri_to_json, w->t->data ), 1, NULL ) );
    }

    if ( !configs.unresolved_ambigs_full_details )
      error_message( "(Re-run LOLS with commandline arguments '--unresolved_ambigs_full_details yes' if you want to see the full details)" );
  }
}

int resolve_one_ambig_label( trie *sht )
{
  trie **iri, *best;
  int max = -1, fTie = 0;

  for ( iri = sht->data; *iri; iri++ )
  {
    char *ont = (*iri)->ont;
    ont_name *n = ont_name_by_str( ont );
    int priority = n->priority;

    if ( priority > max )
    {
      max = priority;
      best = *iri;
      fTie = 0;
    }
    else if ( priority == max )
      fTie = 1;
  }

  if ( fTie )
    return 0;

  free( sht->data );
  CREATE( iri, trie *, 2 );
  iri[0] = best;
  iri[1] = NULL;
  sht->data = iri;

  return 1;
}

int resolve_ambig_labels(void)
{
  trie_wrapper *w, *w_next;
  int fBad = 0;

  for ( w = first_ambig_label; w; w = w->next )
    if ( !resolve_one_ambig_label( w->t ) )
      fBad = 1;

  if ( fBad )
  {
    /*
     * In order to show a more useful diagnostic:
     * remove those labels which were successfully resolved,
     * so only the bad ones will be listed.
     */
    for ( w = first_ambig_label; w; w = w_next )
    {
      w_next = w->next;

      if ( !w->t->data[0] || !w->t->data[1] )
        UNLINK2( w, first_ambig_label, last_ambig_label, next, prev );
    }

    return 0;
  }

  for ( w = first_ambig_label; w; w = w_next )
  {
    w_next = w->next;
    free( w );
  }

  first_ambig_label = last_ambig_label = NULL;

  return 1;
}
