#include "lols.h"
#include "nt_parse.h"

char *lyph_type_as_char( lyph *L );
int top_layer_id;
int top_lyph_id;
trie *blank_nodes;

int load_lyphedges( void )
{
  FILE *fp;
  int line = 1;
  char c, row[MAX_LYPHEDGE_LINE_LEN], *rptr = row, *end = &row[MAX_LYPHEDGE_LINE_LEN - 1], *err = NULL;

  /*
   * Variables for QUICK_GETC
   */
  char read_buf[READ_BLOCK_SIZE], *read_end = &read_buf[READ_BLOCK_SIZE], *read_ptr = read_end;
  int fread_len;

  log_string( "Loading lyphedges..." );

  fp = fopen( "lyphedges.dat", "r" );

  if ( !fp )
  {
    log_string( "Could not open lyphedges.dat for reading" );
    return 0;
  }

  for ( ; ; )
  {
    QUICK_GETC( c, fp );

    if ( !c || c == '\n' || c == '\r' )
    {
      if ( rptr == row )
      {
        if ( c )
        {
          line++;
          continue;
        }
        else
          return 1;
      }

      *rptr = '\0';

      if ( !load_lyphedges_one_line( row, &err ) )
      {
        log_string( "Error while loading lyphedges file." );
        log_linenum( line );
        log_string( err );

        return 0;
      }

      if ( !c )
        return 1;

      line++;
      rptr = row;
    }
    else
    {
      if ( rptr >= end )
      {
        log_string( "Error while loading lyphedges file." );
        log_linenum( line );
        log_string( "Line exceeds maximum length" );

        return 0;
      }

      *rptr++ = c;
    }
  }

  return 1;
}

int load_lyphedges_one_line( char *line, char **err )
{
  char edgeidbuf[MAX_LYPHEDGE_LINE_LEN+1];
  char typebuf[MAX_LYPHEDGE_LINE_LEN+1];
  char fmabuf[MAX_LYPHEDGE_LINE_LEN+1];
  char frombuf[MAX_LYPHEDGE_LINE_LEN+1];
  char tobuf[MAX_LYPHEDGE_LINE_LEN+1];
  char namebuf[MAX_LYPHEDGE_LINE_LEN+1];
  lyphedge *e;
  lyphnode *from, *to;
  trie *etr, *fromtr, *totr;

  #ifdef KEY
  #undef KEY
  #endif
  #define KEY( dest, errmsg )\
    do\
    {\
      if ( !word_from_line( &line, (dest) ) )\
      {\
        *err = (errmsg);\
        return 0;\
      }\
    }\
    while(0)

  KEY( edgeidbuf, "Missing edge ID" );
  KEY( typebuf, "Missing edge type" );
  KEY( fmabuf, "Missing FMA ID" );
  KEY( frombuf, "Missing initial node ID" );
  KEY( tobuf, "Missing terminal node ID" );
  KEY( namebuf, "Missing description" );

  etr = trie_strdup( edgeidbuf, lyphedge_ids );

  if ( !etr->data )
  {
    CREATE( e, lyphedge, 1 );
    e->id = etr;
    etr->data = (trie **)e;
  }
  else
    e = (lyphedge *)etr->data;

  e->type = strtol( typebuf, NULL, 10 );

  if ( e->type < 1 || e->type > 4 )
  {
    *err = "Invalid edge type";
    return 0;
  }

  e->fma = trie_strdup( fmabuf, lyphedge_fmas );

  fromtr = trie_strdup( frombuf, lyphnode_ids );

  if ( !fromtr->data )
  {
    CREATE( from, lyphnode, 1 );

    from->id = fromtr;
    fromtr->data = (trie **)from;
    from->flags = 0;
    CREATE( from->exits, exit_data *, 1 );
    from->exits[0] = NULL;
  }
  else
    from = (lyphnode *)fromtr->data;

  totr = trie_strdup( tobuf, lyphnode_ids );

  if ( !totr->data )
  {
    CREATE( to, lyphnode, 1 );

    to->id = totr;
    totr->data = (trie **)to;
    to->flags = 0;
    CREATE( to->exits, exit_data *, 1 );
    to->exits[0] = NULL;
  }
  else
    to = (lyphnode *)totr->data;

  e->name = trie_strdup( namebuf, lyphedge_names );

  e->from = from;
  e->to = to;
  e->au = NULL;

  add_exit( e, from );

  return 1;
}

void add_exit( lyphedge *e, lyphnode *n )
{
  lyphnode *to;
  exit_data **x;

  if ( e->to == n )
  {
    if ( e->from == n )
      return;

    to = e->from;
  }
  else
    to = e->to;

  if ( n->exits )
  {
    int size;

    for ( x = n->exits; *x; x++ )
      if ( (*x)->to == to )
      {
        /*
         * Possible future to-do: keep track of ALL edges associated with an exit, not just one
         */
        return;
      }

    size = x - n->exits;

    CREATE( x, exit_data *, size + 2 );

    if ( size )
      memcpy( x, n->exits, size * sizeof(exit_data *) );

    CREATE( x[size], exit_data, 1 );
    x[size]->to = to;
    x[size]->via = e;
    x[size+1] = NULL;

    free( n->exits );
    n->exits = x;
  }
  else
  {
    CREATE( x, exit_data *, 2 );
    CREATE( x[0], exit_data, 1 );
    x[0]->to = to;
    x[0]->via = e;
    x[1] = NULL;

    n->exits = x;
  }
}

char *lyphedge_type_str( int type )
{
  switch( type )
  {
    case LYPHEDGE_ARTERIAL:
      return "arterial";
    case LYPHEDGE_MICROCIRC:
      return "microcirculation";
    case LYPHEDGE_VENOUS:
      return "venous";
    case LYPHEDGE_CARDIAC:
      return "cardiac-chamber";
    default:
      return "unknown";
  }
}

int parse_lyph_type_str( char *type )
{
  if ( !strcmp( type, "arterial" ) )
    return LYPHEDGE_ARTERIAL;
  if ( !strcmp( type, "microcirculation" ) )
    return LYPHEDGE_MICROCIRC;
  if ( !strcmp( type, "venous" ) )
    return LYPHEDGE_VENOUS;
  if ( !strcmp( type, "cardiac-chamber" ) )
    return LYPHEDGE_CARDIAC;

  return -1;
}

int word_from_line( char **line, char *buf )
{
  char *lptr = *line, *bptr = buf;

  for ( ; ; )
  {
    switch( *lptr )
    {
      case '\n':
      case '\r':
        return 0;

      case '\t':
      case '\0':
        if ( bptr == buf )
          return 0;

        *bptr = '\0';
        if ( *lptr )
          *line = &lptr[1];
        else
          *line = lptr;

        return 1;

      default:
        *bptr++ = *lptr++;
        continue;
    }
  }
}

void got_lyph_triple( char *subj, char *pred, char *obj )
{
  char *s = subj, *p = pred, *o = obj;

  if ( (*subj == '"' || *subj == '<') && s++ )
    subj[strlen(subj)-1] = '\0';

  if ( (*pred == '"' || *pred == '<') && p++ )
    pred[strlen(pred)-1] = '\0';

  if ( (*obj == '"' || *obj == '<') && o++ )
    obj[strlen(obj)-1] = '\0';

  if ( !strcmp( p, "http://www.w3.org/2000/01/rdf-schema#label" ) )
    load_lyph_label( s, o );
  else if ( !strcmp( p, "http://open-physiology.org/lyph#lyph_type" ) )
    load_lyph_type( s, o );
  else if ( !strcmp( p, "http://open-physiology.org/lyph#has_layers" ) )
    acknowledge_has_layers( s, o );
  else if ( str_begins( p, "http://www.w3.org/1999/02/22-rdf-syntax-ns#_" ) )
    load_layer_to_lld( s, o );
  else if ( !strcmp( p, "http://open-physiology.org/lyph#has_material" ) )
    load_layer_material( s, o );
  else if ( !strcmp( p, "http://open-physiology.org/lyph#has_color" ) )
    load_layer_color( s, o );
  else if ( !strcmp( p, "http://open-physiology.org/lyph#has_thickness" ) )
    load_layer_thickness( s, o );

  free( subj );
  free( pred );
  free( obj );
}

void load_lyph_type( char *subj_full, char *type_str )
{
  char *subj = get_url_shortform( subj_full );
  int type = parse_lyph_type( type_str );
  trie *iri;

  if ( type != -1 )
    iri = trie_search( subj, lyph_ids );

  if ( type == -1 || !iri )
    return;

  ((lyph *)iri->data)->type = type;
}

void load_lyph_label( char *subj_full, char *label )
{
  char *subj = get_url_shortform( subj_full );
  trie *iri;

  if ( !str_begins( subj, "LAYER_" ) )
  {
    lyph *L;

    iri = trie_strdup( subj, lyph_ids );

    if ( !iri->data )
    {
      CREATE( L, lyph, 1 );
      L->id = iri;
      L->type = LYPH_MISSING;
      L->layers = NULL;
      iri->data = (void *)L;
    }
    else
      L = (lyph *)iri->data;

    L->name = trie_strdup( label, lyph_names );
    L->name->data = (void *)L;
  }
}

void acknowledge_has_layers( char *subj_full, char *bnode_id )
{
  char *subj = get_url_shortform( subj_full );
  trie *iri = trie_search( subj, lyph_ids );
  trie *bnode;
  load_layers_data *lld;

  if ( !iri || !iri->data )
    return;

  bnode = trie_strdup( bnode_id, blank_nodes );

  CREATE( lld, load_layers_data, 1 );
  lld->subj = (lyph *)iri->data;
  lld->first_layer_loading = NULL;
  lld->last_layer_loading = NULL;
  lld->layer_count = 0;

  bnode->data = (trie **)lld;
}

void load_layer_to_lld( char *bnode, char *obj_full )
{
  load_layers_data *lld;
  trie *lyr_trie;
  layer *lyr;
  char *obj;
  layer_loading *loading;
  trie *t = trie_search( bnode, blank_nodes );

  if ( !t || !t->data )
    return;

  obj = get_url_shortform( obj_full );

  lyr_trie = trie_search( obj, layer_ids );

  if ( lyr_trie )
    lyr = (layer *)lyr_trie->data;
  else
  {
    CREATE( lyr, layer, 1 );
    lyr->id = trie_strdup( obj, layer_ids );
    lyr->id->data = (trie **)lyr;
    lyr->color = NULL;
    lyr->thickness = -1;
  }

  CREATE( loading, layer_loading, 1 );
  loading->lyr = lyr;

  lld = (load_layers_data *)t->data;

  LINK( loading, lld->first_layer_loading, lld->last_layer_loading, next );
  lld->layer_count++;
}

void load_layer_material( char *subj_full, char *obj_full )
{
  char *subj = get_url_shortform( subj_full );
  char *obj = get_url_shortform( obj_full );
  layer *lyr = layer_by_id( subj );
  lyph *mat;

  if ( !lyr )
    return;

  mat = lyph_by_id( obj );

  if ( !mat )
    return;

  lyr->material = mat;
}

void load_layer_color( char *subj_full, char *obj_full )
{
  char *subj = get_url_shortform( subj_full );
  trie *t;
  layer *lyr;

  t = trie_search( subj, layer_ids );

  if ( !t || !t->data )
    return;

  lyr = (layer *)t->data;

  if ( lyr->color )
    free( lyr->color );

  lyr->color = strdup( obj_full );
}

void load_layer_thickness( char *subj_full, char *obj )
{
  char *subj = get_url_shortform( subj_full );
  trie *t = trie_search( subj, layer_ids );
  layer *lyr;

  if ( !t || !t->data )
    return;

  lyr = (layer *)t->data;

  lyr->thickness = strtol( obj, NULL, 10 );
}

void load_lyphs(void)
{
  FILE *fp;
  char *err = NULL;
  lyph *naked;

  fp = fopen( "lyphs.dat", "r" );

  if ( !fp )
  {
    log_string( "Could not open lyphs.dat for reading" );
    return;
  }

  blank_nodes = blank_trie();

  if ( !parse_ntriples( fp, &err, MAX_IRI_LEN, got_lyph_triple ) )
  {
    char *buf = malloc(strlen(err) + 1024);

    sprintf( buf, "Failed to parse the lyphs-file (lyphs.dat):\n%s\n", err ? err : "(no error given)" );

    error_message( buf );
    abort();
  }

  handle_loaded_layers( blank_nodes );

  if ( (naked=missing_layers( lyph_ids )) != NULL )
  {
    char buf[1024 + MAX_IRI_LEN];

    sprintf( buf, "Error in lyphs.dat: lyph %s has type %s but has no layers\n", trie_to_static( naked->id ), lyph_type_as_char( naked ) );
    error_message( buf );
    abort();
  }
}

lyph *missing_layers( trie *t )
{
  if ( t->data )
  {
    lyph *L = (lyph *)t->data;

    if ( ( L->type == LYPH_MIX || L->type == LYPH_SHELL ) && ( !L->layers || !L->layers[0] ) )
      return L;
  }

  if ( t->children )
  {
    trie **child;

    for ( child = t->children; *child; child++ )
    {
      lyph *L = missing_layers( *child );

      if ( L )
        return L;
    }
  }

  return NULL;
}

void handle_loaded_layers( trie *t )
{
  if ( t->data )
  {
    load_layers_data *lld = (load_layers_data *)t->data;
    layer_loading *load, *load_next;
    lyph *L = lld->subj;
    layer **lyrs, **lptr;

    CREATE( lyrs, layer *, lld->layer_count + 1 );
    lptr = lyrs;

    for ( load = lld->first_layer_loading; load; load = load_next )
    {
      load_next = load->next;
      *lptr++ = load->lyr;
      free( load );
    }

    *lptr = NULL;
    L->layers = lyrs;
  }

  if ( t->children )
  {
    trie **child;

    for ( child = t->children; *child; child++ )
      handle_loaded_layers( *child );

    free( t->children );
  }

  if ( t->label )
    free( t->label );

  free( t );
}

void save_lyphs(void)
{
  FILE *fp;
  trie *avoid_dupe_layers;

  fp = fopen( "lyphs.dat", "w" );

  if ( !fp )
  {
    log_string( "Could not open lyphs.dat for writing" );
    return;
  }

  avoid_dupe_layers = blank_trie();

  save_lyphs_recurse( lyph_ids, fp, avoid_dupe_layers );

  fclose(fp);

  free_lyphdupe_trie( avoid_dupe_layers );

  return;
}

void save_lyphs_recurse( trie *t, FILE *fp, trie *avoid_dupes )
{
  /*
   * Save in N-Triples format for improved interoperability
   */
  static int bnodes;

  if ( t == lyph_ids )
    bnodes = 0;

  if ( t->data )
  {
    lyph *L = (lyph *)t->data;
    char *ch, *id;

    id = id_as_iri( t );
    ch = html_encode( trie_to_static( L->name ) );

    fprintf( fp, "%s <http://www.w3.org/2000/01/rdf-schema#label> \"%s\" .\n", id, ch );
    free( ch );

    fprintf( fp, "%s <http://open-physiology.org/lyph#lyph_type> \"%s\" .\n", id, lyph_type_as_char( L ) );

    if ( L->type == LYPH_SHELL || L->type == LYPH_MIX )
    {
      layer **lyrs;
      int cnt = 1;

      fprintf( fp, "%s <http://open-physiology.org/lyph#has_layers> _:node%d .\n", id, ++bnodes );
      fprintf( fp, "_:node%d <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://www.w3.org/1999/02/22-rdf-syntax-ns#Seq> .\n", bnodes );

      for ( lyrs = L->layers; *lyrs; lyrs++ )
        fprintf_layer( fp, *lyrs, bnodes, cnt++, avoid_dupes );
    }

    free( id );
  }

  if ( t->children )
  {
    trie **child;

    for ( child = t->children; *child; child++ )
      save_lyphs_recurse( *child, fp, avoid_dupes );
  }
}

void fprintf_layer( FILE *fp, layer *lyr, int bnodes, int cnt, trie *avoid_dupes )
{
  char *lid = id_as_iri( lyr->id );
  trie *dupe_search;
  char *mat_iri;

  fprintf( fp, "_:node%d <http://www.w3.org/1999/02/22-rdf-syntax-ns#_%d> %s .\n", bnodes, cnt, lid );

  dupe_search = trie_search( lid, avoid_dupes );

  if ( dupe_search && dupe_search->data )
  {
    free( lid );
    return;
  }

  trie_strdup( lid, avoid_dupes );

  mat_iri = id_as_iri( lyr->material->id );
  fprintf( fp, "%s <http://open-physiology.org/lyph#has_material> %s .\n", lid, mat_iri );
  free( mat_iri );

  if ( lyr->color )
  {
    char *fmtcolor = html_encode( lyr->color );

    fprintf( fp, "%s <http://open-physiology.org/lyph#has_color> \"%s\" .\n", lid, fmtcolor );
    free( fmtcolor );
  }

  if ( lyr->thickness != -1 )
    fprintf( fp, "%s <http://open-physiology.org/lyph#has_thickness> \"%d\" .\n", lid, lyr->thickness );

  free( lid );
}

char *id_as_iri( trie *id )
{
  char *iri = "<http://open-physiology.org/lyphs/#%s>";
  char *retval;
  char *tmp = trie_to_static( id );

  CREATE( retval, char, strlen(iri) + strlen(tmp) + 1 );

  sprintf( retval, iri, tmp );

  return retval;
}

lyph *lyph_by_layers( int type, layer **layers, char *name )
{
  lyph *L;

  if ( type == LYPH_MIX )
    sort_layers( layers );

  L = lyph_by_layers_recurse( type, layers, lyph_ids );

  if ( !L )
  {
    if ( !name )
      return NULL;

    CREATE( L, lyph, 1 );
    L->name = trie_strdup( name, lyph_names );
    L->id = assign_new_lyph_id( L );
    L->type = type;
    L->layers = copy_layers( layers );

    save_lyphs();
  }

  return L;
}

lyph *lyph_by_layers_recurse( int type, layer **layers, trie *t )
{
  if ( t->data )
  {
    lyph *L = (lyph *)t->data;

    if ( L->type == type && same_layers( L->layers, layers ) )
      return L;
  }

  if ( t->children )
  {
    trie **child;
    lyph *L;

    for ( child = t->children; *child; child++ )
    {
      L = lyph_by_layers_recurse( type, layers, *child );

      if ( L )
        return L;
    }
  }

  return NULL;
}

int same_layers( layer **x, layer **y )
{
  for ( ; ; )
  {
    if ( *x != *y )
      return 0;

    if ( !*x )
      return 1;

    x++;
    y++;
  }
}

layer *layer_by_description( char *mtid, int thickness, char *color )
{
  lyph *L = lyph_by_id( mtid );
  layer *lyr;

  if ( !L )
    return NULL;

  if ( thickness < 0 && thickness != -1 )
    return NULL;

  lyr = layer_by_description_recurse( L, thickness, color, layer_ids );

  if ( !lyr )
  {
    CREATE( lyr, layer, 1 );
    lyr->material = L;
    lyr->id = assign_new_layer_id( lyr );
    lyr->thickness = thickness;
    if ( color )
      lyr->color = strdup( color );
    else
      lyr->color = NULL;

    save_lyphs();
  }

  return lyr;
}

trie *assign_new_lyph_id( lyph *L )
{
  top_lyph_id++;
  char buf[128];
  trie *t;

  sprintf( buf, "LYPH_%d", top_lyph_id );

  t = trie_strdup( buf, lyph_ids );

  t->data = (trie **)L;

  return t;
}

trie *assign_new_layer_id( layer *lyr )
{
  top_layer_id++;
  char buf[128];
  trie *t;

  sprintf( buf, "LAYER_%d", top_layer_id );

  t = trie_strdup( buf, layer_ids );

  t->data = (trie **)lyr;

  return t;
}

int layer_matches( layer *candidate, const lyph *material, const float thickness, const char *color )
{
  if ( candidate->material != material )
    return 0;

  if ( thickness != -1 && thickness != candidate->thickness )
    return 0;

  if ( color )
  {
    if ( !candidate->color )
      return 0;

    if ( strcmp( color, candidate->color ) )
      return 0;
  }

  return 1;
}

layer *layer_by_description_recurse( const lyph *L, const float thickness, const char *color, const trie *t )
{
  if ( t->data && layer_matches( (layer *)t->data, L, thickness, color ) )
      return (layer *)t->data;

  if ( t->children )
  {
    trie **child;

    for ( child = t->children; *child; child++ )
    {
      layer *lyr = layer_by_description_recurse( L, thickness, color, *child );
      if ( lyr )
        return lyr;
    }
  }

  return NULL;
}

layer *layer_by_id( char *id )
{
  trie *t = trie_search( id, layer_ids );

  if ( t && t->data )
    return (layer *) t->data;

  return NULL;
}

lyph *lyph_by_name( char *name )
{
  trie *t = trie_search( name, lyph_names );

  if ( t && t->data )
    return (lyph *) t->data;

  return NULL;
}

lyph *lyph_by_id( char *id )
{
  trie *t = trie_search( id, lyph_ids );

  if ( t && t->data )
    return (lyph *) t->data;

  t = trie_search( id, iri_to_labels );

  if ( t && t->data )
  {
    lyph *L;
    char *sht = get_url_shortform( id );

    CREATE( L, lyph, 1 );

    L->type = LYPH_BASIC;
    L->id = trie_strdup( (sht ? sht : id), lyph_ids );
    L->name = trie_strdup( trie_to_static( *t->data ), lyph_names );
    L->layers = NULL;

    L->id->data = (trie **)L;
    L->name->data = (trie **)L;

    save_lyphs();

    return L;
  }

  return NULL;
}

lyphnode *lyphnode_by_id( char *id )
{
  trie *t = trie_search( id, lyphnode_ids );

  if ( t )
    return (lyphnode *)t->data;

  return NULL;
}

lyphedge *lyphedge_by_id( char *id )
{
  trie *t = trie_search( id, lyphedge_ids );

  if ( t )
    return (lyphedge *)t->data;

  return NULL;
}

char *lyph_to_json( lyph *L )
{
  int len;
  char *id, *name, *type, *buf, *bptr;
  char **layers;
  layer **lptr;

  id = json_escape( trie_to_static( L->id ) );
  name = json_escape( trie_to_static( L->name ) );

  len = strlen( "{\"id\": \"\", \"name\": \"\", \"type\": \"\", \"layers\": []}" );

  len += strlen( id );
  len += strlen( name );

  type = lyph_type_as_char( L );
  len += strlen( type );

  if ( L->type != LYPH_BASIC )
  {
    int layercnt;
    char **layerptr;

    for ( lptr = L->layers; *lptr; lptr++ )
      ;

    layercnt = lptr - L->layers;

    CREATE( layers, char *, layercnt + 1 );

    for ( layerptr = layers, lptr = L->layers; *lptr; lptr++ )
    {
      *layerptr = layer_to_json( *lptr );
      len += strlen( *layerptr ) + 1;
      layerptr++;
    }

    *layerptr = NULL;
  }

  CREATE( buf, char, len + 1 );

  sprintf( buf, "{\"id\": \"%s\", \"name\": \"%s\", \"type\": \"%s\", \"layers\": [", id, name, type );
  bptr = &buf[strlen(buf)];

  free( id );
  free( name );

  if ( L->type != LYPH_BASIC )
  {
    char **layerptr;
    int fFirst = 0;

    for ( layerptr = layers; *layerptr; layerptr++ )
    {
      if ( fFirst )
        *bptr++ = ',';
      else
        fFirst = 1;

      sprintf( bptr, "%s", *layerptr );
      bptr = &bptr[strlen(bptr)];
      free( *layerptr );
    }

    free( layers );
  }

  *bptr++ = ']';
  *bptr++ = '}';
  *bptr   = '\0';

  return buf;
}

char *layer_to_json( layer *lyr )
{
  int len;
  char *id, *mtlname, *mtlid, *color, thickness[1024], *buf;

  len = strlen( "{\"id\": \"\", \"mtlname\": \"\", \"mtlid\": \"\", \"color\": \"\", \"thickness\": \"unspecied\"}" );

  id = json_escape( trie_to_static( lyr->id ) );
  mtlname = json_escape( trie_to_static( lyr->material->name ) );
  mtlid = json_escape( trie_to_static( lyr->material->id ) );

  if ( lyr->color )
    color = json_escape( lyr->color );
  else
    color = strdup( "" );

  if ( lyr->thickness != -1 )
    sprintf( thickness, "%d", lyr->thickness );
  else
    sprintf( thickness, "Unspecified" );

  len = len + strlen(id) + strlen(mtlname) + strlen(mtlid) + strlen(color) + strlen( thickness );

  CREATE( buf, char, len+1 );

  sprintf( buf, "{\"id\": \"%s\", \"mtlname\": \"%s\", \"mtlid\": \"%s\", \"color\": \"%s\", \"thickness\": \"%s\"}", id, mtlname, mtlid, color, thickness );

  free( id );
  free( mtlname );
  free( mtlid );
  free( color );

  return buf;
}

char *lyphnode_to_json( lyphnode *n, int include_exits )
{
  int len;
  char *id = json_escape( trie_to_static( n->id ) );
  char **xjson;
  char *buf, *bptr;

  len = strlen( "{\"id\": \"\", \"flags\": \"\", \"exits\": []}" );

  len = len + strlen(id) + 1024;

  if ( include_exits )
  {
    char **xjsptr;
    exit_data **x;

    for ( x = n->exits; *x; x++ )
      ;

    CREATE( xjson, char *, x - n->exits );

    for ( x = n->exits, xjsptr = xjson; *x; x++ )
    {
      *xjsptr = exit_to_json( *x );
      len += strlen( *xjsptr ) + strlen( "," );
      xjsptr++;
    }
  }

  CREATE( buf, char, len+1 );

  sprintf( buf, "{\"id\": \"%s\", \"flags\": \"%d\"", id, n->flags );
  bptr = &buf[strlen(buf)];
  free( id );

  if ( include_exits )
  {
    char **xjsptr;
    exit_data **x;
    int fFirst = 0;

    sprintf( bptr, ", \"exits\": [" );
    bptr = &bptr[strlen(bptr)];

    for ( x = n->exits, xjsptr = xjson; *x; x++ )
    {
      if ( fFirst )
        *bptr++ = ',';
      else
        fFirst = 1;

      sprintf( bptr, "%s", *xjsptr );
      free( *xjsptr++ );
      bptr = &bptr[strlen(bptr)];
    }

    free( xjson );
    *bptr++ = ']';
  }

  *bptr++ = '}';
  *bptr = '\0';

  return buf;
}

char *exit_to_json( exit_data *x )
{
  char *to_id = json_escape( trie_to_static( x->to->id ) );
  char *via_id = json_escape( trie_to_static( x->via->id ) );
  char *json;
  int len;

  len = strlen( "{\"to\": \"\", \"via\": \"\"}" );
  len += strlen( to_id );
  len += strlen( via_id );

  CREATE( json, char, len + 1 );

  sprintf( json, "{\"to\": \"%s\", \"via\": \"%s\"}", to_id, via_id );

  free( to_id );
  free( via_id );

  return json;
}

char *lyphedge_to_json( lyphedge *e )
{
  int len;
  char *id = json_escape( trie_to_static( e->id ) );
  char *fma = json_escape( trie_to_static( e->fma ) );
  char *name = json_escape( trie_to_static( e->name ) );
  char *from = lyphnode_to_json( e->from, 0 );
  char *to = lyphnode_to_json( e->to, 0 );
  char *au;
  char *json;

  if ( e->au )
  {
    char *quoted;

    au = json_escape( trie_to_static( e->au->id ) );
    CREATE( quoted, char, strlen(au) + strlen( "\"\"" ) + 1 );
    sprintf( quoted, "\"%s\"", au );
    free( au );
    au = quoted;
  }
  else
    au = "null";

  len = strlen( "{\"id\": \"\", \"fma\": \"\", \"name\": \"\", \"type\": \"\", \"from\": , \"to\": , \"au\": }" );
  len += strlen( id ) + strlen( fma ) + strlen( name ) + 1024 + strlen( from ) + strlen( to ) + strlen( au );

  CREATE( json, char, len+1 );

  sprintf( json, "{\"id\": \"%s\", \"fma\": \"%s\", \"name\": \"%s\", \"type\": \"%d\", \"from\": %s, \"to\": %s, \"au\": %s}", id, fma, name, e->type, from, to, au );

  free( id );
  free( fma );
  free( name );
  free( from );
  free( to );
  if ( e->au )
    free( au );

  return json;
}

char *lyphpath_to_json( lyphedge **path )
{
  lyphedge **ptr;
  char **edges, **edgesptr, *buf, *bptr;
  int steps, len, fFirst = 0;

  len = strlen( "{\"length\": \"\", \"edges\": []}" ) + 1024;

  for ( ptr = path; *ptr; ptr++ )
    ;

  steps = ptr - path;

  CREATE( edges, char *, steps );

  for ( ptr = path, edgesptr = edges; *ptr; ptr++ )
  {
    *edgesptr = lyphedge_to_json( *ptr );
    len += strlen( *edgesptr ) + strlen( "," );
    edgesptr++;
  }

  CREATE( buf, char, len + 1 );

  sprintf( buf, "{\"length\": \"%d\", \"edges\": [", steps );
  bptr = &buf[strlen(buf)];

  for ( ptr = path, edgesptr = edges; *ptr; ptr++ )
  {
    if ( fFirst )
      *bptr++ = ',';
    else
      fFirst = 1;

    sprintf( bptr, "%s", *edgesptr );
    bptr = &bptr[strlen(bptr)];

    free( *edgesptr );
    edgesptr++;
  }

  free( edges );

  *bptr++ = ']';
  *bptr++ = '}';
  *bptr = '\0';

  return buf;
}

char *lyph_type_as_char( lyph *L )
{
  switch( L->type )
  {
    case LYPH_BASIC:
      return "basic";
    case LYPH_SHELL:
      return "shell";
    case LYPH_MIX:
      return "mix";
    default:
      return "unknown";
  }
}

layer **copy_layers( layer **src )
{
  int len = layers_len( src );
  layer **dest;

  CREATE( dest, layer *, len + 1 );

  memcpy( dest, src, sizeof( layer * ) * (len + 1) );

  return dest;
}

int layers_len( layer **layers )
{
  layer **ptr;

  for ( ptr = layers; *ptr; ptr++ )
    ;

  return ptr - layers;
}

/*
 * cmp_layers could be optimized; right now we'll
 * operate under the assumption that an AU doesn't have
 * hundreds+ of layers, so optimizing this is deprioritized
 */
int cmp_layers(const void * a, const void * b)
{
  const layer *x;
  const layer *y;
  char buf[MAX_STRING_LEN+1];

  if ( a == b )
    return 0;

  x = *((const layer **) a);
  y = *((const layer **) b);

  sprintf( buf, "%s", trie_to_static( x->id ) );

  return strcmp( buf, trie_to_static( y->id ) );
}

void sort_layers( layer **layers )
{
  qsort( layers, layers_len( layers ), sizeof(layer *), cmp_layers );

  return;
}

void free_lyphdupe_trie( trie *t )
{
  if ( t->children )
  {
    trie **child;

    for ( child = t->children; *child; child++ )
      free_lyphdupe_trie( *child );

    free( t->children );
  }

  if ( t->label )
    free( t->label );

  free( t );
}

int parse_lyph_type( char *str )
{
  if ( !strcmp( str, "mix" ) )
    return LYPH_MIX;

  if ( !strcmp( str, "shell" ) )
    return LYPH_SHELL;

  if ( !strcmp( str, "basic" ) )
    return LYPH_BASIC;

  return -1;
}

lyphedge **compute_lyphpath( lyphnode *from, lyphnode *to )
{
  lyphstep *head = NULL, *tail = NULL, *step, *curr;

  if ( from == to )
  {
    lyphedge **path;

    CREATE( path, lyphedge *, 1 );
    path[0] = NULL;

    return path;
  }

  CREATE( step, lyphstep, 1 );
  step->depth = 0;
  step->backtrace = NULL;
  step->location = from;
  step->edge = NULL;

  LINK2( step, head, tail, next, prev );
  curr = step;
  from->flags |= LYPHNODE_SEEN;

  for ( ; ; curr = curr->next )
  {
    exit_data **x;

    if ( !curr )
    {
      free_lyphsteps( head );
      return NULL;
    }

    if ( curr->location == to )
    {
      lyphedge **path, **pptr;

      CREATE( path, lyphedge *, curr->depth + 1 );
      pptr = &path[curr->depth-1];
      path[curr->depth] = NULL;

      do
      {
        *pptr-- = curr->edge;
        curr = curr->backtrace;
      }
      while( curr->backtrace );

      free_lyphsteps( head );
      return path;
    }

    for ( x = curr->location->exits; *x; x++ )
    {
      if ( (*x)->to->flags & LYPHNODE_SEEN )
        continue;

      CREATE( step, lyphstep, 1 );
      step->depth = curr->depth + 1;
      step->backtrace = curr;
      step->location = (*x)->to;
      step->edge = (*x)->via;
      LINK2( step, head, tail, next, prev );
      step->location->flags |= LYPHNODE_SEEN;
    }
  }

  return NULL;
}

void free_lyphsteps( lyphstep *head )
{
  lyphstep *step, *step_next;

  for ( step = head; step; step = step_next )
  {
    step_next = step->next;

    step->location->flags &= ~(LYPHNODE_SEEN);
    free( step );
  }
}
