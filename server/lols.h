#include "macro.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_STRING_LEN 64000
#define READ_BLOCK_SIZE 1048576  // 1024 * 1024.  For QUICK_GETC.
#define MAX_AUTOCOMPLETE_RESULTS_PRESORT 30
#define MAX_AUTOCOMPLETE_RESULTS_POSTSORT 10

//#define LOLS_WINDOWS

/*
 * Typedefs
 */
typedef struct TRIE trie;
typedef struct TRIE_WRAPPER trie_wrapper;
typedef struct UCL_SYNTAX ucl_syntax;
typedef struct AMBIG ambig;

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

struct TRIE_WRAPPER
{
  trie_wrapper *next;
  trie_wrapper *prev;
  trie *t;
};

struct UCL_SYNTAX
{
  int type;
  ucl_syntax *sub1;
  ucl_syntax *sub2;
  char *reln;
  trie *iri;
  char *toString;
};

typedef enum
{
  UCL_BLANK, UCL_SYNTAX_BASE, UCL_SYNTAX_PAREN, UCL_SYNTAX_SOME, UCL_SYNTAX_AND, UCL_SYNTAX_OR, UCL_SYNTAX_NOT
} ucl_syntax_types;

struct AMBIG
{
  ambig *next;
  ambig *prev;
  trie **data;
  char *label;
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
trie **get_autocomplete_labels( char *label_ch, int case_insens );

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
void trie_search_autocomplete( char *label_ch, trie **buf, trie *base );
int cmp_trie_data (const void * a, const void * b);

/*
 * util.c
 */
void log_string( char *txt );
char *html_encode( char *str );
void init_html_codes( void );
char *lowercaserize( char *x );
char *get_url_shortform( char *iri );
char *url_decode(char *str);
int is_number( const char *arg );

/*
 * ucl.c
 */
ucl_syntax *parse_ucl_syntax( char *ucl, char **err, char **maybe_err, ambig **ambig_head, ambig **ambig_tail );
int str_begins( char *full, char *init );
char *read_some_relation( char *left, char *right );
void kill_ucl_syntax( ucl_syntax *s );
int is_ambiguous( trie **data );
void free_ambigs( ambig *head );
char *ucl_syntax_output( ucl_syntax *s, ambig *head, ambig *tail, char *possible_error );
