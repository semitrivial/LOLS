//#define LOLS_WINDOWS
//#define LOLS_OSX
#define LOLS_UNIX_CMDLINE

#define ENABLE_LYPHS

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

/*
 * Typedefs
 */
typedef struct TRIE trie;
typedef struct TRIE_WRAPPER trie_wrapper;
typedef struct UCL_SYNTAX ucl_syntax;
typedef struct AMBIG ambig;
typedef struct LYPH lyph;
typedef struct LAYER layer;
typedef struct LAYER_LOADING layer_loading;
typedef struct LOAD_LAYERS_DATA load_layers_data;

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

struct LYPH
{
  trie *name;
  trie *id;
  int type;
  layer **layers;
};

typedef enum
{
  LYPH_BASIC, LYPH_SHELL, LYPH_MIX, LYPH_MISSING
} lyph_types;

struct LAYER
{
  lyph *material;
  char *color;
  int thickness;
  trie *id;
};

struct LOAD_LAYERS_DATA
{
  lyph *subj;
  layer_loading *first_layer_loading;
  layer_loading *last_layer_loading;
  int layer_count;
};

struct LAYER_LOADING
{
  layer_loading *next;
  layer_loading *prev;
  layer *lyr;
};

/*
 * Global variables
 */
extern trie *iri_to_labels;
extern trie *label_to_iris;
extern trie *label_to_iris_lowercase;

extern trie *lyph_names;
extern trie *lyph_ids;
extern trie *layer_ids;

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
void error_message( char *err );

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

/*
 * lyph.c
 */
lyph *lyph_by_name( char *name );
lyph *lyph_by_id( char *id );
char *lyph_to_json( lyph *L );
char *layer_to_json( layer *lyr );
layer *layer_by_id( char *id );
layer *layer_by_description( char *mtid, int thickness, char *color );
layer *layer_by_description_recurse( const lyph *L, const float thickness, const char *color, const trie *t );
trie *assign_new_layer_id( layer *lyr );
lyph *lyph_by_layers( int type, layer **layers, char *name );
lyph *lyph_by_layers_recurse( int type, layer **layers, trie *t );
int same_layers( layer **x, layer **y );
layer **copy_layers( layer **src );
int layers_len( layer **layers );
void sort_layers( layer **layers );
trie *assign_new_lyph_id( lyph *L );
void free_lyphdupe_trie( trie *t );
void save_lyphs_recurse( trie *t, FILE *fp, trie *avoid_dupes );
char *id_as_iri( trie *id );
void fprintf_layer( FILE *fp, layer *lyr, int bnodes, int cnt, trie *avoid_dupes );
void load_lyphs( void );
int parse_lyph_type( char *str );
void load_lyph_label( char *subj_full, char *label );
void load_lyph_type( char *subj_full, char *type_str );
void acknowledge_has_layers( char *subj_full, char *bnode_id, trie *bnodes );
void load_layer_color( char *subj_full, char *obj_full );
void load_layer_to_lld( char *bnode, char *obj_full, trie *bnodes );
void load_layer_thickness( char *subj_full, char *obj );
