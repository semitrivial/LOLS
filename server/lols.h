#include "macro.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_STRING_LEN 64000
#define READ_BLOCK_SIZE 1048576  // 1024 * 1024.  For QUICK_GETC.

/*
 * Typedefs
 */
typedef struct TRIE trie;

/*
 * Structures
 */
struct TRIE
{
  trie *parent;
  char *label;
  trie **children;
  trie **data;
};

/*
 * Global variables
 */
extern trie *iri_to_labels;
extern trie *label_to_iris;
extern trie *label_to_iris_lowercase;

/*
 * Function prototypes
 */

/*
 * lols.c
 */
void init_lols(void);
void parse_lols_file(FILE *fp);
void add_lols_entry( char *iri_ch, char *label_ch );
trie **get_labels_by_iri( char *iri_ch );
trie **get_iris_by_label( char *label_ch );
trie **get_iris_by_label_case_insensitive( char *label_ch );

/*
 * srv.c
 */
void main_loop(void);

/*
 * trie.c
 */
trie *blank_trie(void);
trie *trie_strdup( char *buf, trie *base );
trie *trie_search( char *buf, trie *base );
char *trie_to_static( trie *t );

/*
 * util.c
 */
void log_string( char *txt );
char *html_encode( char *str );
void init_html_codes( void );
char *lowercaserize( char *x );
char *get_url_shortform( char *iri );
char *url_decode(char *str);
