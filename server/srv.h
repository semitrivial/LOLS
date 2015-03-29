#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#if !defined(FNDELAY)
#define FNDELAY O_NDELAY
#endif

/*
 * If a browser connects, but doesn't do anything, how long until kicking them off
 * (See also the next comment below)
 */
#define HTTP_KICK_IDLE_AFTER_X_SECS 60

/*
 * How many times, per second, will your main project be checking for new connections?
 * (Rather than keep track of the exact time a client is idle, rather we keep track of
 *  the number of times we've checked for updates and found none.  When the client has
 *  been idle for HTTP_KICK_IDLE_AFTER_X_SECS * HTTP_PULSE_PER_SEC consecutive checks,
 *  they will be booted.  HTTP_PULSES_PER_SEC is not used anywhere else.  Thus, it is
 *  not terribly important that it be completely precise, a ballpark estimate is good
 *  enough.
 */
#define HTTP_PULSES_PER_SEC 10

#define HTTP_INITIAL_OUTBUF_SIZE 16384
#define HTTP_INITIAL_INBUF_SIZE 16384
#define HTTP_MAX_INBUF_SIZE 131072
#define HTTP_MAX_OUTBUF_SIZE 524288

#define HTTP_LISTEN_BACKLOG 32

#define HTTP_SOCKSTATE_READING_REQUEST 0
#define HTTP_SOCKSTATE_WRITING_RESPONSE 1
#define HTTP_SOCKSTATE_AWAITING_INSTRUCTIONS 2

/*
 * Macros
 */

#define HND_ERR_NORETURN( x )\
do\
{\
  char *jsonerr = JSON1( "Error": x );\
  send_200_response( req, jsonerr );\
}\
while(0)

#define HND_ERR( x )\
do\
{\
  HND_ERR_NORETURN( x );\
  return;\
}\
while(0)

/*
 * Structures
 */
typedef struct HTTP_REQUEST http_request;
typedef struct HTTP_CONN http_conn;
typedef struct URL_PARAM url_param;
typedef struct PRED_RESULT pred_result;

struct HTTP_REQUEST
{
  http_request *next;
  http_request *prev;
  http_conn *conn;
  char *query;
  char *callback;  //JSONP support
  int *dead;
};

struct HTTP_CONN
{
  http_conn *next;
  http_conn *prev;
  http_request *req;
  int sock;
  int state;
  int idle;
  char *buf;
  int bufsize;
  int buflen;
  char *outbuf;
  int outbufsize;
  int outbuflen;
  int len;
  char *writehead;
};

struct URL_PARAM
{
  char *key;
  char *val;
};

struct PRED_RESULT
{
  trie *label;
  trie *iri;
  char *ont;
};

/*
 * Global variables
 */
http_request *first_http_req;
http_request *last_http_req;
http_conn *first_http_conn;
http_conn *last_http_conn;

fd_set http_inset;
fd_set http_outset;
fd_set http_excset;

int srvsock;

/*
 * Local function prototypes
 */
void init_lols_http_server( int port );
void http_update_connections( void );
void http_kill_socket( http_conn *c );
void free_http_request( http_request *r );
void http_answer_the_phone( int srvsock );
int resize_buffer( http_conn *c, char **buf );
void http_listen_to_request( http_conn *c );
void http_flush_response( http_conn *c );
void http_parse_input( http_conn *c );
http_request *http_recv( void );
void http_write( http_request *req, char *txt );
void http_send( http_request *req, char *txt, int len );
void send_400_response( http_request *req );
void send_200_response( http_request *req, char *txt );
void send_200_with_type( http_request *req, char *txt, char *type );
char *nocache_headers(void);
char *current_date(void);
void send_gui( http_request *req );
void send_js( http_request *req );
char *load_file( char *filename );
const char *parse_params( char *buf, int *fShortIRI, int *fCaseInsens, http_request *req, url_param **params );
void handle_ucl_syntax_request( char *request, http_request *req );
void free_url_params( url_param **buf );
char *get_url_param( url_param **params, char *key );
void handle_all_predicates_request( http_request *req, char *request, url_param **params );
void handle_predicate_request( http_request *req, char *request, url_param **params );
void handle_predicate_autocomplete_request( http_request *req, char *request, url_param **params );
char *get_param( url_param **params, char *key );
int parse_commandline_args( int argc, const char* argv[], const char **filename, const char **configfile, int *port, int *unambig );
int parse_config_file( const char *filename );
void default_config_values( void );
