#include "lols.h"

void init_lols(void)
{
  iri_to_labels = blank_trie();
  label_to_iris = blank_trie();
  label_to_iris_lowercase = blank_trie();

  init_html_codes();

  return;
}

void parse_lols_file(FILE *fp)
{
  char c;
  char iri[MAX_STRING_LEN], *iriptr = iri;
  char label[MAX_STRING_LEN], *lblptr;
  int fSpace = 0;
  int linenum = 1;

  /*
   * Variables for QUICK_GETC
   */
  char read_buf[READ_BLOCK_SIZE], *read_end = &read_buf[READ_BLOCK_SIZE], *read_ptr = read_end;
  int fread_len;

  printf( "Parsing file...\n" );

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
        fprintf( stderr, "Error on line %d of lols-file: blank IRI\n\n", linenum );
        abort();
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

  printf( "Finished parsing file.\n" );

  return;
}

void add_to_data( trie ***dest, trie *datum )
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
    for ( data = *dest, cnt = 0; *data; data++ )
      cnt++;

    CREATE( data, trie *, cnt + 2 );
    memcpy( data, *dest, cnt * sizeof(trie*) );
    data[cnt] = datum;
    free( *dest );
    *dest = data;
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
