//#define LOLS_WINDOWS
//#define LOLS_OSX
#define LOLS_UNIX_CMDLINE

#include "mallocf.h"
#include "macro.h"
#include "jsonfmt.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_STRING_LEN 64000
#define READ_BLOCK_SIZE 1048576  // 1024 * 1024.  For QUICK_GETC.
#define MAX_IRI_LEN 2048
#define MAX_AUTOCOMPLETE_RESULTS_PRESORT 30
#define MAX_AUTOCOMPLETE_RESULTS_POSTSORT 10
#define MAX_URL_PARAMS 32
#define MAX_URL_PARAM_LEN 512
#define MAX_INT_LEN (strlen("-2147483647"))

/*
 * Typedefs
 */
typedef char * (*json_array_printer) (void *what, void *how);
typedef struct TRIE trie;
typedef struct TRIE_WRAPPER trie_wrapper;
typedef struct UCL_SYNTAX ucl_syntax;
typedef struct AMBIG ambig;
typedef struct ONT_NAME ont_name;
typedef struct CONFIG_VALUES config_values;

/*
 * Structures
 */
struct TRIE
{
  trie *parent;
  char *label;
  trie **children;
  trie **data;
  char *ont;
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

struct ONT_NAME
{
  ont_name *next;
  ont_name *prev;
  char *namespace;
  char *friendly;
  int priority;
  int ignore_ambigs;
};

struct CONFIG_VALUES
{
  int unresolved_ambigs_full_details;
};

/*
 * Global variables
 */
extern trie *iri_to_labels;
extern trie *label_to_iris;
extern trie *label_to_iris_lowercase;
extern trie *predicates_full;
extern trie *predicates_short;

extern int unambig_mode;
extern config_values configs;
extern ont_name *first_ont_name;
extern ont_name *last_ont_name;

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
void add_lols_predicate( char *iri_ch, char *label );
ont_name *ont_name_by_str( char *str );
char *get_ont_by_iri( char *full );
void mark_ambiguous_label( trie *label );
void display_unresolved_ambig_labels( void );
int resolve_ambig_labels(void);

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
char *trie_to_json( trie *t );
void trie_search_autocomplete( char *label_ch, trie **buf, trie *base );
int cmp_trie_data (const void * a, const void * b);
void **datas_to_array( trie *t );
int count_nontrivial_members( trie *t );

/*
 * util.c
 */
void log_string( char *txt );
void log_linenum( int linenum );
char *html_encode( char *str );
void init_html_codes( void );
char *lowercaserize( const char *x );
void lowercaserize_destructive( char *x );
char *get_url_shortform( char *iri );
char *url_decode(char *str);
int is_number( const char *arg );
void error_message( char *err );
void error_messagef( char *err, ... );
char *pretty_free( char *json );
char *strdupf( const char *fmt, ... );
char *jsonf( int paircnt, ... );;
char *jslist_r( json_array_printer *p, void **array, void *param );
size_t voidlen( void **x );
char *label_to_iri_to_json( trie *label );
int full_matches_ont( char *full, char *ont );

/*
 * ucl.c
 */
ucl_syntax *parse_ucl_syntax( char *ucl, char **err, char **maybe_err, ambig **ambig_head, ambig **ambig_tail );
int str_approx( char *full, char *init );
int str_begins( char *full, char *init );
char *read_some_relation( char *left, char *right );
void kill_ucl_syntax( ucl_syntax *s );
int is_ambiguous( trie **data );
void free_ambigs( ambig *head );
char *ucl_syntax_output( ucl_syntax *s, ambig *head, ambig *tail, char *possible_error );
char *ont_from_full( char *full );
