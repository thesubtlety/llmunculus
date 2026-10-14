// remote model backend, selected by JB_MODEL_URL. engine.c dispatches to these when an engine is URL-backed.
// state is a messages array replayed on every request, since a chat endpoint is stateless per call.
#pragma once

typedef struct http_engine http_engine;

http_engine *http_open(const char *url);          // NULL on bad url / out of memory
void http_close(http_engine *);
int  http_feed(http_engine *, const char *text);  // seed the system message (first and only feed)
int  http_turn_user(http_engine *, const char *text);
int  http_turn_end(http_engine *);                // commit the last reply as the assistant turn
int  http_gen(http_engine *, const char *gbnf, float temp, unsigned seed, int max_tok, char *out, unsigned long cap);
int  http_pos(http_engine *);                      // message count, the rewind bookmark
void http_rewind(http_engine *, int pos);
int  http_last_gen(http_engine *);                 // rough token count of the last reply, for logs
