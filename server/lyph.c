#include "lols.h"

char *lyph_type_as_char( lyph *L );
int top_layer_id;
int top_lyph_id;

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
  int bnodes = 0;

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

  fprintf( fp, "_:node%d <http://www.w3.org/1999/02/22-rdf-syntax-ns#_%d> %s .\n", bnodes, cnt, lid );

  dupe_search = trie_search( lid, avoid_dupes );

  if ( dupe_search && dupe_search->data )
    return;

  trie_strdup( lid, avoid_dupes );

  if ( lyr->color )
  {
    char *fmtcolor = html_encode( lyr->color );

    fprintf( fp, "%s <http://open-physiology.org/lyph#has_color> \"%s\" .\n", lid, fmtcolor );
    free( fmtcolor );
  }

  if ( lyr->thickness != -1 )
    fprintf( fp, "%s <http://open-physiology.org/lyph#has_thickness> \"%f\" .\n", lid, lyr->thickness );
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

layer *layer_by_description( char *mtid, float thickness, char *color )
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

layer *layer_by_description_recurse( const lyph *L, const float thickness, const char *color, const trie *t )
{
  if ( t->data )
  {
    layer *lyr = (layer *)t->data;

    if ( lyr->material == L )
    {
      if ( ( thickness == -1 || thickness == lyr->thickness )
      &&   ( !color || !strcmp( color, lyr->color ) ) )
        return lyr;
    }
  }

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

char *lyph_to_json( lyph *L )
{
  int len;
  char *id, *name, *type, *buf, *bptr, *pretty;
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

  pretty = json_format( buf, 2, NULL );
  free( buf );

  return pretty;
}

char *layer_to_json( layer *lyr )
{
  int len;
  char *id, *mtlname, *mtlid, *color, *buf;

  len = strlen( "{\"id\": \"\", \"mtlname\": \"\", \"mtlid\": \"\", \"color\": \"\"}" );

  id = json_escape( trie_to_static( lyr->id ) );
  mtlname = json_escape( trie_to_static( lyr->material->name ) );
  mtlid = json_escape( trie_to_static( lyr->material->id ) );

  if ( lyr->color )
    color = json_escape( lyr->color );
  else
    color = strdup( "" );

  len = len + strlen(id) + strlen(mtlname) + strlen(mtlid) + strlen(color);

  CREATE( buf, char, len+1 );

  sprintf( buf, "{\"id\": \"%s\", \"mtlname\": \"%s\", \"mtlid\": \"%s\", \"color\": \"%s\"}", id, mtlname, mtlid, color );

  free( id );
  free( mtlname );
  free( mtlid );
  free( color );

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
  const layer *x = (const layer *) a;
  const layer *y = (const layer *) b;
  char buf[MAX_STRING_LEN+1];

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
