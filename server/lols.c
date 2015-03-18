#include "lols.h"
#include "nt_parse.h"

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
}

void old_parse_lols_file(FILE *fp)
{
  char c;
  char iri[MAX_STRING_LEN], *iriptr = iri;
  char label[MAX_STRING_LEN], *lblptr = label;
  int fSpace = 0;
  int linenum = 1;

  /*
   * Variables for QUICK_GETC
   */
  char read_buf[READ_BLOCK_SIZE], *read_end = &read_buf[READ_BLOCK_SIZE], *read_ptr = read_end;
  int fread_len;

  log_string( "Parsing file...\n" );

  for(;;)
  {
    QUICK_GETC(c,fp);

    if ( !c )
    {
      /*
       * To do: fix this
      if ( (!fSpace && iriptr != iri) || (fSpace && lblptr == label) )
      {
        lacking_label:
        fprintf( stderr, "Error: last line of lols-file lacks a label\n\n" );
        abort();
      }
       */
      break;
    }

    if ( c == '\n' )
    {
      /*
       *
      if ( !fSpace )
        goto lacking_label;
       */

      *lblptr = '\0';
      add_lols_entry( iri, label );
      iriptr = iri;
      fSpace = 0;
      linenum++;
      continue;
    }

    if ( !fSpace && c == ' ' )
    {
      if ( iriptr == iri )
      {
        char buf[1024];

        sprintf( buf, "Error on line %d of lols-file: blank IRI\n\n", linenum );
        error_message( buf );

        exit(EXIT_SUCCESS);
      }

      *iriptr = '\0';
      lblptr = label;
      fSpace = 1;
      continue;
    }

    if ( fSpace )
      *lblptr++ = c;
    else
      *iriptr++ = c;
  }

  log_string( "Finished parsing file.\n" );

  return;
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

  lowercaserize_destructive( iri_ch );
  ont = ont_from_full( iri_ch );

  if ( ont )
    full->data = (trie **)strdup( ont );
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
    add_to_data( &iri_shortform->data, label );
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
