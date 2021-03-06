#include "lols.h"
#include "srv.h"

char *html;
char *js;
int unambig_mode = 0;
config_values configs;

int main( int argc, const char* argv[] )
{
  FILE *fp;
  const char *configfile = NULL;
  const char *filename = NULL;
  int port=5052;

  default_config_values();

  if ( !parse_commandline_args( argc, argv, &filename, &configfile, &port, &unambig_mode ) )
    return 0;

  fp = fopen( filename, "r" );

  if ( !fp )
  {
    fprintf( stderr, "Could not open file %s for reading\n\n", filename );
    return 0;
  }

  if ( configfile )
  {
    if ( !parse_config_file( configfile ) )
      return 0;
  }

  init_lols();
  init_lols_http_server(port);

  parse_lols_file(fp);

  fclose(fp);

  printf( "Ready.\n" );

  while(1)
  {
    /*
     * To do: make this commandline-configurable
     */
    usleep(100000);
    main_loop();
  }
}

void init_lols_http_server( int port )
{
  int status, yes=1;
  struct addrinfo hints, *servinfo;
  char portstr[128];

  html = load_file( "gui.html" );
  js = load_file( "gui.js" );

  memset( &hints, 0, sizeof(hints) );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  sprintf( portstr, "%d", port );

  if ( (status=getaddrinfo(NULL,portstr,&hints,&servinfo)) != 0 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (getaddrinfo failed)\n" );
    abort();
  }

  srvsock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol );

  if ( srvsock == -1 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (socket failed)\n" );
    abort();
  }

  if ( setsockopt( srvsock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) ) == -1 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (setsockopt failed)\n" );
    abort();
  }

  if ( bind( srvsock, servinfo->ai_addr, servinfo->ai_addrlen ) == -1 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (bind failed)\n" );
    exit( EXIT_SUCCESS );
  }

  if ( listen( srvsock, HTTP_LISTEN_BACKLOG ) == -1 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (listen failed)\n" );
    abort();
  }

  freeaddrinfo( servinfo );

  return;
}

void main_loop( void )
{
  http_request *req;
  int count=0;

  while ( count < 10 )
  {
    req = http_recv();

    if ( req )
    {
      char *reqptr, *reqtype, *request, repl[MAX_STRING_LEN], *rptr;
      const char *parse_params_err;
      trie **data;
      int len=0, fFirst=0, fShortIRI=0, fCaseInsens=0;
      url_param *params[MAX_URL_PARAMS+1];

      count++;

      if ( !strcmp( req->query, "gui" )
      ||   !strcmp( req->query, "/gui" )
      ||   !strcmp( req->query, "gui/" )
      ||   !strcmp( req->query, "/gui/" ) )
      {
        send_gui( req );
        continue;
      }

      if ( !strcmp( req->query, "js/" )
      ||   !strcmp( req->query, "/js/" ) )
      {
        send_js( req );
        continue;
      }

      for ( reqptr = (*req->query == '/') ? req->query + 1 : req->query; *reqptr; reqptr++ )
        if ( *reqptr == '/' )
          break;

      if ( !*reqptr )
      {
        send_400_response( req );
        continue;
      }

      *reqptr = '\0';
      reqtype = (*req->query == '/') ? req->query + 1 : req->query;

      parse_params_err = parse_params( &reqptr[1], &fShortIRI, &fCaseInsens, req, params );

      if ( parse_params_err )
      {
        char errmsg[1024];

        sprintf( errmsg, "{\"Error\": \"%s\"}", parse_params_err );

        send_200_response( req, errmsg );
        free_url_params( params );
        continue;
      }

      request = url_decode(&reqptr[1]);

      if ( !strcmp( reqtype, "uclsyntax" )
      ||   !strcmp( reqtype, "ucl_syntax" )
      ||   !strcmp( reqtype, "ucl-syntax" ) )
      {
        handle_ucl_syntax_request( request, req );
        free_url_params( params );
        free( request );
        continue;
      }

      if ( !strcmp( reqtype, "iri" ) )
        data = get_labels_by_iri( request );
      else if ( !strcmp( reqtype, "label" ) && !fCaseInsens )
        data = get_iris_by_label( request );
      else if ( !strcmp( reqtype, "label" )
           ||   !strcmp( reqtype, "label-case-insensitive" ) )
        data = get_iris_by_label_case_insensitive( request );
      else if ( !strcmp( reqtype, "label-shortiri" ) )
      {
        data = get_iris_by_label( request );
        fShortIRI = 1;
      }
      else if ( !strcmp( reqtype, "label-shortiri-case-insensitive" ) || !strcmp( reqtype, "label-case-insensitive-shortiri" ) )
      {
        data = get_iris_by_label_case_insensitive( request );
        fShortIRI = 1;
      }
      else if ( !strcmp( reqtype, "autocomp" ) || !strcmp( reqtype, "autocomplete" ) )
      {
        data = get_autocomplete_labels( request, 0, params );

        send_200_response( req, JSON
        (
          "Results": JS_ARRAY( trie_to_json, data ),
          "IRIs": JS_ARRAY( label_to_iri_to_json, data )
        ) );

        goto main_loop_cleanup;
      }
      else if ( !strcmp( reqtype, "autocomp-case-insensitive" ) || !strcmp( reqtype, "autocomplete-case-insensitive" ) )
      {
        data = get_autocomplete_labels( request, 1, params );

        send_200_response( req, JSON
        (
          "Results": JS_ARRAY( trie_to_json, data ),
          "IRIs": JS_ARRAY( label_to_iri_to_json, data )
        ) );

        goto main_loop_cleanup;
      }
      else if ( !strcmp( reqtype, "ontologies" ) )
      {
        handle_ontologies_request( req, request, params );
        goto main_loop_cleanup;
      }
      else if ( !strcmp( reqtype, "all_predicates" ) )
      {
        handle_all_predicates_request( req, request, params );
        goto main_loop_cleanup;
      }
      else if ( !strcmp( reqtype, "predicate" ) )
      {
        handle_predicate_request( req, request, params );
        goto main_loop_cleanup;
      }
      else if ( !strcmp( reqtype, "predicate_autocomplete" ) )
      {
        handle_predicate_autocomplete_request( req, request, params );
        goto main_loop_cleanup;
      }
      else
      {
        send_400_response( req );

        main_loop_cleanup:
        free_url_params( params );
        *reqptr = '/';
        free( request );
        continue;
      }

      if ( !data )
      {
        send_200_response( req, "{\"Results\": []}" );

        goto main_loop_cleanup;
      }

      free_url_params( params );

      sprintf( repl, "{\"Results\": [" );
      rptr = &repl[strlen( "{\"Results\": [")];

      for ( ; *data; data++ )
      {
        char *datum, *encoded;
        int datumlen;

        if ( fShortIRI )
        {
          char *longform = trie_to_static( *data );
          datum = get_url_shortform( longform );
          if ( !datum )
            datum = longform;
        }
        else
          datum = trie_to_static( *data );

        encoded = html_encode( datum );

        datumlen = strlen( encoded ) + strlen("\"\"") + (fFirst ? strlen(",") : 0);

        if ( len + datumlen >= MAX_STRING_LEN - 10 )
        {
          sprintf( rptr, ",\"...\"" );
          free( encoded );
          break;
        }
        len += datumlen;
        sprintf( rptr, "%s\"%s\"", fFirst ? "," : "", encoded );
        rptr = &rptr[datumlen];
        free( encoded );
        fFirst = 1;
      }
      sprintf( rptr, "]}" );

      send_200_response( req, repl );

      goto main_loop_cleanup;
    }
    else
      break;
  }

  json_gc();
}

char *get_param( url_param **params, char *key )
{
  url_param **ptr;

  for ( ptr = params; *ptr; ptr++ )
    if ( !strcmp( (*ptr)->key, key ) )
      return (*ptr)->val;

  return NULL;
}

void http_update_connections( void )
{
  static struct timeval zero_time;
  http_conn *c, *c_next;
  int top_desc;

  FD_ZERO( &http_inset );
  FD_ZERO( &http_outset );
  FD_ZERO( &http_excset );
  FD_SET( srvsock, &http_inset );
  top_desc = srvsock;

  for ( c = first_http_conn; c; c = c->next )
  {
    if ( c->sock > top_desc )
      top_desc = c->sock;

    if ( c->state == HTTP_SOCKSTATE_READING_REQUEST )
      FD_SET( c->sock, &http_inset );
    else
      FD_SET( c->sock, &http_outset );

    FD_SET( c->sock, &http_excset );
  }

  /*
   * Poll sockets
   */
  if ( select( top_desc+1, &http_inset, &http_outset, &http_excset, &zero_time ) < 0 )
  {
    fprintf( stderr, "Fatal: select failed to poll the sockets\n" );
    abort();
  }

  if ( !FD_ISSET( srvsock, &http_excset ) && FD_ISSET( srvsock, &http_inset ) )
    http_answer_the_phone( srvsock );

  for ( c = first_http_conn; c; c = c_next )
  {
    c_next = c->next;

    c->idle++;

    if ( FD_ISSET( c->sock, &http_excset )
    ||   c->idle > HTTP_KICK_IDLE_AFTER_X_SECS * HTTP_PULSES_PER_SEC )
    {
      FD_CLR( c->sock, &http_inset );
      FD_CLR( c->sock, &http_outset );

      http_kill_socket( c );
      continue;
    }

    if ( c->state == HTTP_SOCKSTATE_AWAITING_INSTRUCTIONS )
      continue;

    if ( c->state == HTTP_SOCKSTATE_READING_REQUEST
    &&   FD_ISSET( c->sock, &http_inset ) )
    {
      c->idle = 0;
      http_listen_to_request( c );
    }
    else
    if ( c->state == HTTP_SOCKSTATE_WRITING_RESPONSE
    &&   c->outbuflen > 0
    &&   FD_ISSET( c->sock, &http_outset ) )
    {
      c->idle = 0;
      http_flush_response( c );
    }
  }
}

void http_kill_socket( http_conn *c )
{
  UNLINK2( c, first_http_conn, last_http_conn, next, prev );

  free( c->buf );
  free( c->outbuf );
  free_http_request( c->req );

  close( c->sock );

  free( c );
}

void free_http_request( http_request *r )
{
  if ( !r )
    return;

  if ( r->dead )
    *r->dead = 1;

  UNLINK2( r, first_http_req, last_http_req, next, prev );

  if ( r->query )
    free( r->query );

  if ( r->callback )
    free( r->callback );

  free( r );
}

void http_answer_the_phone( int srvsock )
{
  struct sockaddr_storage their_addr;
  socklen_t addr_size = sizeof(their_addr);
  int caller;
  http_conn *c;
  http_request *req;

  if ( ( caller = accept( srvsock, (struct sockaddr *) &their_addr, &addr_size ) ) < 0 )
    return;

  if ( ( fcntl( caller, F_SETFL, FNDELAY ) ) == -1 )
  {
    close( caller );
    return;
  }

  CREATE( c, http_conn, 1 );
  c->next = NULL;
  c->sock = caller;
  c->idle = 0;
  c->state = HTTP_SOCKSTATE_READING_REQUEST;
  c->writehead = NULL;

  CREATE( c->buf, char, HTTP_INITIAL_INBUF_SIZE + 1 );
  c->bufsize = HTTP_INITIAL_INBUF_SIZE;
  c->buflen = 0;
  *c->buf = '\0';

  CREATE( c->outbuf, char, HTTP_INITIAL_OUTBUF_SIZE + 1 );
  c->outbufsize = HTTP_INITIAL_OUTBUF_SIZE + 1;
  c->outbuflen = 0;
  *c->outbuf = '\0';

  CREATE( req, http_request, 1 );
  req->next = NULL;
  req->conn = c;
  req->query = NULL;
  req->callback = NULL;
  c->req = req;

  LINK2( req, first_http_req, last_http_req, next, prev );
  LINK2( c, first_http_conn, last_http_conn, next, prev );
}

int resize_buffer( http_conn *c, char **buf )
{
  int max, *size;
  char *tmp;

  if ( *buf == c->buf )
  {
    max = HTTP_MAX_INBUF_SIZE;
    size = &c->bufsize;
  }
  else
  {
    max = HTTP_MAX_OUTBUF_SIZE;
    size = &c->outbufsize;
  }

  *size *= 2;
  if ( *size >= max )
  {
    http_kill_socket(c);
    return 0;
  }

  CREATE( tmp, char, (*size)+1 );

  sprintf( tmp, "%s", *buf );
  free( *buf );
  *buf = tmp;
  return 1;
}

void http_listen_to_request( http_conn *c )
{
  int start = c->buflen, readsize;

  if ( start >= c->bufsize - 5
  &&  !resize_buffer( c, &c->buf ) )
    return;

  readsize = recv( c->sock, c->buf + start, c->bufsize - 5 - start, 0 );

  if ( readsize > 0 )
  {
    c->buflen += readsize;
    http_parse_input( c );
    return;
  }

  if ( readsize == 0 || errno != EWOULDBLOCK )
    http_kill_socket( c );
}

void http_flush_response( http_conn *c )
{
  int sent_amount;

  if ( !c->writehead )
    c->writehead = c->outbuf;

  sent_amount = send( c->sock, c->writehead, c->outbuflen, 0 );

  if ( sent_amount >= c->outbuflen )
  {
    http_kill_socket( c );
    return;
  }

  c->outbuflen -= sent_amount;
  c->writehead = &c->writehead[sent_amount];
}

void http_parse_input( http_conn *c )
{
  char *bptr, *end, query[MAX_STRING_LEN], *qptr;
  int spaces = 0, chars = 0;

  end = &c->buf[c->buflen];

  for ( bptr = c->buf; bptr < end; bptr++ )
  {
    switch( *bptr )
    {
      case ' ':
        spaces++;

        if ( spaces == 2 )
        {
          *qptr = '\0';
          c->req->query = strdup( query );
          c->state = HTTP_SOCKSTATE_AWAITING_INSTRUCTIONS;
          return;
        }
        else
        {
          *query = '\0';
          qptr = query;
        }

        break;

      case '\0':
      case '\n':
      case '\r':
        http_kill_socket( c );
        return;

      default:
        if ( spaces != 0 )
        {
          if ( ++chars >= MAX_STRING_LEN - 10 )
          {
            http_kill_socket( c );
            return;
          }
          *qptr++ = *bptr;
        }
        break;
    }
  }
}

http_request *http_recv( void )
{
  http_request *req, *best = NULL;
  int max_idle = -1;

  http_update_connections();

  for ( req = first_http_req; req; req = req->next )
  {
    if ( req->conn->state == HTTP_SOCKSTATE_AWAITING_INSTRUCTIONS
    &&   req->conn->idle > max_idle )
    {
      max_idle = req->conn->idle;
      best = req;
    }
  }

  if ( best )
  {
    best->conn->state = HTTP_SOCKSTATE_WRITING_RESPONSE;
    return best;
  }

  return NULL;
}

void http_write( http_request *req, char *txt )
{
  http_send( req, txt, strlen(txt) );
}

/*
 * Warning: can crash if txt is huge.  Up to caller
 * to ensure txt is not too huge.
 */
void http_send( http_request *req, char *txt, int len )
{
  http_conn *c = req->conn;

  if ( len >= c->outbufsize - 5 )
  {
    char *newbuf;

    CREATE( newbuf, char, len+10 );
    memcpy( newbuf, txt, len+1 );
    free( c->outbuf );
    c->outbuf = newbuf;
    c->outbuflen = len;
    c->outbufsize = len+10;
  }
  else
  {
    memcpy( c->outbuf, txt, len );
    c->outbuflen = len;
  }
}

void send_400_response( http_request *req )
{
  char buf[MAX_STRING_LEN];

  sprintf( buf, "HTTP/1.1 400 Bad Request\r\n"
                "Date: %s\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "%s"
                "Content-Length: %zd\r\n"
                "\r\n"
                "Syntax Error",
                current_date(),
                nocache_headers(),
                strlen( "Syntax Error" ) );

  http_write( req, buf );
}

void send_200_response( http_request *req, char *txt )
{
  send_200_with_type( req, txt, "application/json" );
}

void send_200_with_type( http_request *req, char *txt, char *type )
{
  char *buf;

  /*
   * JSONP support
   */
  if ( req->callback )
  {
    char *jsonp = malloc( strlen(txt) + strlen(req->callback) + strlen( "(\n\n);" ) + 1 );

    sprintf( jsonp, "%s(\n%s\n);", req->callback, txt );
    txt = jsonp;
  }

  buf = strdupf(  "HTTP/1.1 200 OK\r\n"
                  "Date: %s\r\n"
                  "Content-Type: %s; charset=utf-8\r\n"
                  "%s"
                  "Content-Length: %zd\r\n"
                  "\r\n"
                  "%s",
                  current_date(),
                  type,
                  nocache_headers(),
                  strlen(txt),
                  txt );

  if ( req->callback )
    free( txt );

  http_write( req, buf );

  free( buf );
}

char *nocache_headers(void)
{
  return "Cache-Control: no-cache, no-store, must-revalidate\r\n"
         "Pragma: no-cache\r\n"
         "Expires: 0\r\n";
}

char *current_date(void)
{
  time_t rawtime;
  struct tm *timeinfo;
  static char buf[2048];
  char *bptr;

  time ( &rawtime );
  timeinfo = localtime( &rawtime );
  sprintf( buf, "%s", asctime ( timeinfo ) );

  for ( bptr = &buf[strlen(buf)-1]; *bptr == '\n' || *bptr == '\r'; bptr-- )
    ;

  bptr[1] = '\0';

  return buf;
}

void send_gui( http_request *req )
{
  send_200_with_type( req, html, "text/html" );
}

void send_js( http_request *req )
{
  send_200_with_type( req, js, "application/javascript" );
}

char *load_file( char *filename )
{
  FILE *fp;
  char *buf, *bptr;
  int size;

  /*
   * Variables for QUICK_GETC
   */
  char read_buf[READ_BLOCK_SIZE], *read_end = &read_buf[READ_BLOCK_SIZE], *read_ptr = read_end;
  int fread_len;

  fp = fopen( filename, "r" );

  if ( !fp )
  {
    fprintf( stderr, "Fatal: Couldn't open %s for reading\n", filename );
    abort();
  }

  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  CREATE( buf, char, size+1 );

  for ( bptr = buf; ; bptr++ )
  {
    QUICK_GETC(*bptr,fp);

    if ( !*bptr )
      return buf;
  }
}

const char *parse_params( char *buf, int *fShortIRI, int *fCaseInsens, http_request *req, url_param **params )
{
  char *bptr;
  char *param;
  int fEnd, cnt=0;
  url_param **pptr = params;

  for ( bptr = buf; *bptr; bptr++ )
    if ( *bptr == '?' )
      break;

  if ( !*bptr )
  {
    *params = NULL;
    return NULL;
  }

  *bptr++ = '\0';
  param = bptr;
  fEnd = 0;

  for (;;)
  {
    if ( *bptr == '&' || *bptr == '\0' )
    {
      if ( *bptr == '\0' )
        fEnd = 1;
      else
        *bptr = '\0';

      if ( ++cnt >= MAX_URL_PARAMS )
      {
        *pptr = NULL;
        return "Too many URL parameters";
      }

      if ( !strcmp( param, "case-insensitive" )
      ||   !strcmp( param, "case-ins" )
      ||   !strcmp( param, "insensitive" )
      ||   !strcmp( param, "ins" ) )
        *fCaseInsens = 1;
      else
      if ( !strcmp( param, "short-iri" )
      ||   !strcmp( param, "short" ) )
        *fShortIRI = 1;
      else
      {
        char *equals;

        for ( equals = param; *equals; equals++ )
          if ( *equals == '=' )
            break;

        if ( *equals )
        {
          *equals = '\0';

          if ( strlen( param ) >= MAX_URL_PARAM_LEN )
          {
            *pptr = NULL;
            return "Url parameter too long";
          }

          CREATE( *pptr, url_param, 1 );
          (*pptr)->key = url_decode( param );
          (*pptr)->val = url_decode( &equals[1] );

          if ( !strcmp( (*pptr)->key, "callback" ) )
          {
            if ( req->callback )
              free( req->callback );

            req->callback = strdup( (*pptr)->val );
          }

          pptr++;
        }
      }

      if ( fEnd )
      {
        *pptr = NULL;
        return NULL;
      }

      param = &bptr[1];
    }

    bptr++;
  }
}

void handle_ucl_syntax_request( char *request, http_request *req )
{
  ucl_syntax *s;
  char *err = NULL, *maybe_err = NULL, *output;
  ambig *head = NULL, *tail = NULL;

  s = parse_ucl_syntax( request, &err, &maybe_err, &head, &tail );

  if ( !s )
  {
    if ( err )
    {
      HND_ERR_NORETURN( err );
      free( err );
    }
    else
      HND_ERR_NORETURN( "Malformed UCL Syntax" );

    if ( head )
      free_ambigs( head );
    if ( maybe_err )
      free( maybe_err );

    return;
  }

  output = ucl_syntax_output( s, head, tail, maybe_err );

  send_200_response( req, output );

  free( output );

  kill_ucl_syntax( s );

  if ( head )
    free_ambigs( head );
  if ( maybe_err )
    free( maybe_err );

  return;
}

void free_url_params( url_param **buf )
{
  url_param **ptr;

  for ( ptr = buf; *ptr; ptr++ )
  {
    free( (*ptr)->key );
    free( (*ptr)->val );
    free( *ptr );
  }
}

char *get_url_param( url_param **params, char *key )
{
  url_param **ptr;

  for ( ptr = params; *ptr; ptr++ )
    if ( !strcmp( (*ptr)->key, key ) )
      return (*ptr)->val;

  return NULL;
}

void populate_predicate_results( trie *t, pred_result ***bptr, char *ont )
{
  if ( t->data )
  {
    trie **d;

    for ( d = t->data; *d; d++ )
    {
      pred_result *r;

      if ( ont )
      {
        char *d_ont = (*d)->ont;

        if ( strcmp( d_ont, ont ) )
          continue;
      }

      CREATE( r, pred_result, 1 );

      r->label = t;
      r->iri = *d;
      r->ont = (*d)->ont;

      **bptr = r;
      (*bptr)++;
    }
  }

  TRIE_RECURSE( populate_predicate_results( *child, bptr, ont ) );
}

char *predicate_label_to_json( pred_result *r )
{
  return JSON
  (
    "label": trie_to_json( r->label ),
    "ontology": r->ont,
    "iri": trie_to_json( r->iri )
  );
}

void free_pred_results( pred_result **buf )
{
  pred_result **ptr;

  for ( ptr = buf; *ptr; ptr++ )
    free( *ptr );
}

void handle_ontologies_request( http_request *req, char *request, url_param **params )
{
  ont_name *n, *n2;
  char *buf, *bptr, *formatted;
  int cnt = 0, fFirst = 0;

  for ( n = first_ont_name; n; n = n->next )
    cnt++;

  CREATE( buf, char, ((cnt+1) * 2048) + 1 );
  sprintf( buf, "{\"ontologies\":[" );
  bptr = &buf[strlen(buf)];

  for ( n = first_ont_name; n; n = n->next )
  {
    char *escaped;
    int fFirstIRI;

    for ( n2 = first_ont_name; n2; n2 = n2->next )
    {
      if ( n2 == n )
        break;

      if ( !strcmp( n2->friendly, n->friendly ) )
        break;
    }

    if ( n2 != n )
      continue;

    if ( fFirst )
      *bptr++ = ',';
    else
      fFirst = 1;

    escaped = json_escape( n->friendly );

    sprintf( bptr, "{\"shortform\": \"%s\",\"iris\": [", escaped );
    free( escaped );
    bptr += strlen(bptr);

    for ( fFirstIRI = 0, n2 = n; n2; n2 = n2->next )
    {
      if ( strcmp( n2->friendly, n->friendly ) )
        continue;

      if ( fFirstIRI )
        *bptr++ = ',';
      else
        fFirstIRI = 1;

      escaped = json_escape( n2->namespace );
      sprintf( bptr, "\"%s\"", escaped );
      bptr += strlen( bptr );
      free( escaped );
    }
    *bptr++ = ']';
    *bptr++ = '}';
  }

  *bptr++ = ']';
  *bptr++ = '}';
  *bptr++ = '\0';

  formatted = json_format( buf, 2, NULL );

  send_200_response( req, formatted ? formatted : buf );

  free( buf );
}

void handle_all_predicates_request( http_request *req, char *request, url_param **params )
{
  char *ont = get_param( params, "ont" );
  static pred_result *buf[1024*1024];
  pred_result **bptr;

  bptr = buf;

  populate_predicate_results( predicates_short, &bptr, ont );

  *bptr = NULL;

  send_200_response( req, JSON1
  (
    "results": JS_ARRAY( predicate_label_to_json, buf )
  ) );

  free_pred_results( buf );
}

void handle_predicate_request( http_request *req, char *request, url_param **params )
{
  return;
}

void handle_predicate_autocomplete_request( http_request *req, char *request, url_param **params )
{
  return;
}

int parse_commandline_args( int argc, const char* argv[], const char **filename, const char **configfile, int *port, int *unambig )
{
  argc--;
  argv++;

  if ( !(argc % 2) )
  {
    parse_commandline_args_help:
    printf( "Syntax: lols [optional arguments] <file>\n" );
    printf( "\n" );
    printf( "Optional arguments are as follows:\n" );
    printf( "  -p <portnum>\n" );
    printf( "    Specify which port LOLS will listen on.\n" );
    printf( "    (Default: 5052)\n" );
    printf( "\n" );
    printf( "  -unambig <yes or no>\n" );
    printf( "    Specify whether LOLS should operate in 'unambiguous' mode\n" );
    printf( "    (Default: no)\n" );
    printf( "\n" );
    printf( "  -config <configfile>\n" );
    printf( "    Specify a configfile for LOLS\n" );
    printf( "    (Default: no config file)\n" );
    printf( "\n" );
    printf( "  -help\n" );
    printf( "    Displays this helpfile\n" );
    printf( "\n" );

    return 0;
  }

  for ( ; argc > 1; argc -= 2, argv += 2 )
  {
    const char *param = argv[0];

    while ( *param == '-' )
      param++;

    param = lowercaserize( param );

    if ( !strcmp( param, "p" ) )
    {
      int portnum = strtoul( argv[1], NULL, 10 );

      if ( portnum < 1 )
      {
        fprintf( stderr, "Port number should be a positive integer\n" );
        return 0;
      }

      *port = portnum;
      printf( "LOLS has been set to listen on port %d.\n", portnum );
      continue;
    }

    if ( !strcmp( param, "unambig" ) )
    {
      if ( !strcmp( argv[1], "yes" ) )
      {
        *unambig = 1;
        printf( "LOLS has been set to use unambiguous mode.\n" );
        continue;
      }

      if ( !strcmp( argv[1], "no" ) )
      {
        *unambig = 0;
        printf( "LOLS has been set to not use unambiguous mode.\n" );
        continue;
      }

      fprintf( stderr, "Error: 'unambig' should be set as either 'yes' or 'no'\n" );
      return 0;
    }

    if ( !strcmp( param, "config" ) )
    {
      *configfile = argv[1];
      printf( "LOLS has been set to use [%s] as its config file.\n", argv[1] );
      continue;
    }

    if ( !strcmp( param, "unresolved_ambigs_full_details" ) )
    {
      if ( !strcmp( argv[1], "yes" ) )
        configs.unresolved_ambigs_full_details = 1;
      else if ( strcmp( argv[1], "no" ) )
      {
        fprintf( stderr, "The 'unresolved_ambigs_full_details' parameter must be set to 'yes' or 'no'\n" );
        return 0;
      }

      continue;
    }

    if ( !strcmp( param, "help" ) )
      goto parse_commandline_args_help;

    fprintf( stderr, "Unrecognized parameter: [%s]\n", param );
    return 0;
  }

  *filename = argv[0];

  return 1;
}

void parse_ignore_ontology( char *s, int line, const char *filename )
{
  ont_name *which = ont_name_by_str( s );

  if ( !which )
  {
    error_messagef( "Line %d of file %s:\n", line, filename );
    error_messagef( "Ontology [%s] was not recognized\n", s );
    EXIT();
  }

  which->ignore = 1;
}

void parse_ignore_ambigs( char *s, int line, const char *filename )
{
  ont_name *which;

  which = ont_name_by_str( s );

  if ( !which )
  {
    error_messagef( "Line %d of file %s:\n", line, filename );
    error_messagef( "Ontology [%s] was not recognized\n", s );
    EXIT();
  }

  which->ignore_ambigs = 1;
}

void parse_priority( char *s, int line, const char *filename )
{
  ont_name *high_priority, *low_priority;
  char *gt, *left, *right;

  for ( gt = s; *gt; gt++ )
    if ( *gt == '>' )
      break;

  if ( !*gt )
  {
    error_messagef( "Line %d of file %s:\n", line, filename );
    error_message( "No > found.\n" );
    EXIT();
  }

  right = &gt[1];

  while ( *right == ' ' )
    right++;

  while ( gt > s && gt[-1] == ' ' )
    gt--;

  *gt = '\0';

  left = s;

  high_priority = ont_name_by_str( left );

  if ( !high_priority )
  {
    error_messagef( "Line %d of file %s:\n", line, filename );
    error_messagef( "Ontology [%s] could not be found\n", left );
    EXIT();
  }

  low_priority = ont_name_by_str( right );

  if ( !low_priority )
  {
    error_messagef( "Line %d of file %s:\n", line, filename );
    error_messagef( "Ontology [%s] could not be found\n", right );
    EXIT();
  }

  if ( low_priority == high_priority )
    return;

  if ( low_priority->priority + 1 > high_priority->priority )
    high_priority->priority = low_priority->priority + 1;
}

int parse_config_file( const char *filename )
{
  FILE *fp = fopen( filename, "r" );
  int line = 0;

  /*
   * Variables for QUICK_GETC
   */
  char read_buf[READ_BLOCK_SIZE], *read_end = &read_buf[READ_BLOCK_SIZE], *read_ptr = read_end;
  int fread_len;

  if ( !fp )
  {
    fprintf( stderr, "Error: Could not open configfile [%s] for reading\n", filename );
    return 0;
  }

  for ( ; ; )
  {
    ont_name *n;
    char buf[MAX_STRING_LEN], *bptr, c, *space, *friendly, *namespace;

    QUICK_GETLINE( buf, bptr, c, fp );

    if ( !bptr )
      break;

    line++;

    if ( !*buf || *buf == '#' )
      continue;

    for ( space = buf; *space; space++ )
      if ( *space == ' ' )
        break;

    if ( !*space )
    {
      error_messagef( "Line %d of file %s:\n", line, filename );
      error_messagef( "No space found\n" );
      EXIT();
    }

    *space = '\0';

    if ( !strcmp( buf, "Priority" ) )
    {
      parse_priority( &space[1], line, filename );
      continue;
    }

    if ( !strcmp( buf, "IgnoreAmbigs" ) )
    {
      parse_ignore_ambigs( &space[1], line, filename );
      continue;
    }

    if ( !strcmp( buf, "IgnoreOntology" ) )
    {
      parse_ignore_ontology( &space[1], line, filename );
      continue;
    }

    namespace = buf;
    friendly = &space[1];

    for ( n = first_ont_name; n; n = n->next )
      if ( !strcmp( n->friendly, friendly ) )
        break;

    if ( n )
      friendly = n->friendly;
    else
      friendly = strdup( friendly );

    CREATE( n, ont_name, 1 );
    n->friendly = friendly;
    n->namespace = strdup( namespace );
    n->priority = 0;
    n->ignore_ambigs = 0;
    n->ignore = 0;
    LINK2( n, first_ont_name, last_ont_name, next, prev );
  }

  fclose( fp );
  return 1;
}

void default_config_values(void)
{
  configs.unresolved_ambigs_full_details = 0;
}
