// talk to a chat endpoint (ollama, or an openai-compatible server) instead of a local GGUF. the loop drives this
// through engine.h exactly as the local engine. we keep the conversation as a messages array and resend it whole
// each call, because the endpoint holds no state. flavor picks the request shape and how a grammar is passed, if at all.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine_http.h"
#include "grammar.h"   // identify the step's grammar by pointer, to pick its JSON schema and reformat the reply

extern int jb_http (const char *method, const char *url, const char *body, const char *ctype, char *out, unsigned long cap);
extern int jb_https(const char *method, const char *url, const char *body, const char *ctype, char *out, unsigned long cap);

enum { F_OLLAMA, F_OPENAI, F_LLAMACPP, F_VLLM };
#define MAXMSG 128

struct http_engine {
    char base[512], model[128];
    int  flavor, n, last_gen;
    char *role[MAXMSG], *text[MAXMSG];
    char pending[8192];   // last reply, until http_turn_end commits it
};

http_engine *http_open(const char *url) {
    if (!url || (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))) return NULL;
    http_engine *h = calloc(1, sizeof *h);
    if (!h) return NULL;
    snprintf(h->base, sizeof h->base, "%s", url);
    size_t bl = strlen(h->base); if (bl && h->base[bl - 1] == '/') h->base[bl - 1] = 0;
    const char *api = getenv("JB_MODEL_API");
    h->flavor = F_OLLAMA;
    if (api) { if (!strcmp(api, "openai")) h->flavor = F_OPENAI; else if (!strcmp(api, "llamacpp")) h->flavor = F_LLAMACPP; else if (!strcmp(api, "vllm")) h->flavor = F_VLLM; }
    const char *name = getenv("JB_MODEL_NAME");
    snprintf(h->model, sizeof h->model, "%s", name && *name ? name : "default");
    return h;
}

void http_close(http_engine *h) {
    if (!h) return;
    for (int i = 0; i < h->n; i++) { free(h->role[i]); free(h->text[i]); }
    free(h);
}

static int push(http_engine *h, const char *role, const char *text) {
    if (h->n >= MAXMSG) return -1;
    h->role[h->n] = strdup(role); h->text[h->n] = strdup(text);
    if (!h->role[h->n] || !h->text[h->n]) return -1;
    h->n++; return 0;
}

int http_feed(http_engine *h, const char *text) { return h->n == 0 ? push(h, "system", text) : 0; }
int http_turn_user(http_engine *h, const char *text) { return push(h, "user", text); }
int http_turn_end(http_engine *h) { return push(h, "assistant", h->pending); }
int http_pos(http_engine *h) { return h->n; }
int http_last_gen(http_engine *h) { return h->last_gen; }
void http_rewind(http_engine *h, int pos) { for (int i = pos; i < h->n; i++) { free(h->role[i]); free(h->text[i]); } if (pos >= 0 && pos < h->n) h->n = pos; }

// a growing string, so a long conversation does not need a fixed buffer.
typedef struct { char *p; size_t len, cap; } sb;
static int sb_need(sb *s, size_t extra) {
    if (s->len + extra + 1 <= s->cap) return 0;
    size_t c = s->cap ? s->cap : 1024;
    while (s->len + extra + 1 > c) c *= 2;
    char *np = realloc(s->p, c); if (!np) return -1;
    s->p = np; s->cap = c; return 0;
}
static int sb_raw(sb *s, const char *t) { size_t n = strlen(t); if (sb_need(s, n)) return -1; memcpy(s->p + s->len, t, n); s->len += n; s->p[s->len] = 0; return 0; }
static int sb_json(sb *s, const char *t) {   // append t as an escaped json string, without the surrounding quotes
    for (; *t; t++) { unsigned char c = *t; const char *e = 0; char b[8];
        switch (c) { case '"': e = "\\\""; break; case '\\': e = "\\\\"; break; case '\n': e = "\\n"; break; case '\r': e = "\\r"; break; case '\t': e = "\\t"; break; }
        if (e) { if (sb_raw(s, e)) return -1; }
        else if (c < 0x20) { snprintf(b, sizeof b, "\\u%04x", c); if (sb_raw(s, b)) return -1; }
        else { if (sb_need(s, 1)) return -1; s->p[s->len++] = c; s->p[s->len] = 0; }
    }
    return 0;
}

static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

// decode the json string value that follows `"content"`: skip to ':', the opening quote, then unescape until the close.
static int json_str_after(const char *at, char *out, size_t cap) {
    const char *p = strchr(at, ':'); if (!p) return -1;
    for (p++; *p == ' ' || *p == '\t'; p++) {}
    if (*p != '"') return -1;
    size_t o = 0;
    for (p++; *p && *p != '"'; p++) {
        if (o + 4 >= cap) break;
        if (*p != '\\') { out[o++] = *p; continue; }
        p++;
        switch (*p) {
            case 'n': out[o++] = '\n'; break; case 't': out[o++] = '\t'; break; case 'r': out[o++] = '\r'; break;
            case 'b': out[o++] = '\b'; break; case 'f': out[o++] = '\f'; break;
            case '"': case '\\': case '/': out[o++] = *p; break;
            case 'u': {   // \uXXXX -> utf-8
                int v = 0, ok = 1;
                for (int k = 0; k < 4; k++) { int hv = p[1] ? hexval(p[1]) : -1; if (hv < 0) { ok = 0; break; } v = v * 16 + hv; p++; }
                if (!ok) break;
                if (v < 0x80) out[o++] = v;
                else if (v < 0x800) { out[o++] = 0xC0 | (v >> 6); out[o++] = 0x80 | (v & 0x3F); }
                else { out[o++] = 0xE0 | (v >> 12); out[o++] = 0x80 | ((v >> 6) & 0x3F); out[o++] = 0x80 | (v & 0x3F); }
                break;
            }
            default: out[o++] = *p;
        }
        if (!*p) break;
    }
    out[o] = 0; return (int)o;
}

static int words(const char *s) { int n = 0, in = 0; for (; *s; s++) { if (*s == ' ' || *s == '\n' || *s == '\t') in = 0; else if (!in) { in = 1; n++; } } return n; }

// phase 2: ollama and generic openai endpoints have no GBNF, only JSON Schema. for the grammars whose structure
// matters, send a schema and convert the JSON reply back into the exact line format the loop's parsers expect, so
// the loop and parse.c stay unchanged. llama.cpp/vllm keep their GBNF (better) and never reach here.
#define PLAN_SCHEMA(mn, mx) "{\"type\":\"object\",\"properties\":{\"steps\":{\"type\":\"array\",\"minItems\":" mn ",\"maxItems\":" mx \
    ",\"items\":{\"type\":\"object\",\"properties\":{\"kind\":{\"type\":\"string\",\"enum\":[\"NEED\",\"DO\"]},\"text\":{\"type\":\"string\"}}," \
    "\"required\":[\"kind\",\"text\"]}}},\"required\":[\"steps\"]}"

static const char *schema_for(const char *g) {
    if (g == G_PLAN_ONE)  return PLAN_SCHEMA("1", "1");
    if (g == G_PLAN)      return PLAN_SCHEMA("1", "3");
    if (g == G_PLAN_NONE) return PLAN_SCHEMA("0", "3");
    if (g == G_CODE)      return "{\"type\":\"object\",\"properties\":{\"code\":{\"type\":\"string\"}},\"required\":[\"code\"]}";
    if (g == G_VERDICT)   return "{\"type\":\"object\",\"properties\":{\"verdict\":{\"type\":\"string\",\"enum\":[\"FACT\",\"FIX\"]},\"fix\":{\"type\":\"string\"}},\"required\":[\"verdict\"]}";
    if (g == G_OBSERVE)   return "{\"type\":\"object\",\"properties\":{\"kind\":{\"type\":\"string\",\"enum\":[\"FACT\",\"FIX\"]},\"text\":{\"type\":\"string\"}},\"required\":[\"kind\",\"text\"]}";
    if (g == G_YESNO)     return "{\"type\":\"object\",\"properties\":{\"answer\":{\"type\":\"string\",\"enum\":[\"yes\",\"no\",\"unknown\"]}},\"required\":[\"answer\"]}";
    return NULL;   // G_LINES, G_ANSWER, free text: schema-free, no reformat
}

static int schema_off(void) { const char *v = getenv("JB_MODEL_SCHEMA"); return v && (!strcmp(v, "0") || !strcmp(v, "off") || !strcmp(v, "no")); }

// json reply for grammar g -> the line text parse.c expects. falls back to the raw json (parser then retries).
static void reformat(const char *g, const char *j, char *out, size_t cap) {
    char a[256], b[256];
    if (g == G_PLAN || g == G_PLAN_ONE || g == G_PLAN_NONE) {
        size_t o = 0; out[0] = 0;
        for (const char *p = strstr(j, "\"kind\""); p; p = strstr(p + 6, "\"kind\"")) {   // schema order is kind then text
            if (json_str_after(p, a, sizeof a) < 0) break;
            const char *tp = strstr(p, "\"text\"");
            if (!tp || json_str_after(tp, b, sizeof b) < 0) break;
            int w = snprintf(out + o, o < cap ? cap - o : 0, "%s%s\n", strcmp(a, "DO") ? "NEED: " : "DO: ", b);
            if (w < 0) break;
            o += w; if (o >= cap) break;
            p = tp;
        }
        if (o == 0) snprintf(out, cap, "NONE");   // empty plan -> the loop's "done/informed NONE" path
        return;
    }
    if (g == G_CODE) {
        const char *cp = strstr(j, "\"code\"");
        int pre = snprintf(out, cap, "```c\n");
        int n = cp ? json_str_after(cp, out + pre, (size_t)pre < cap ? cap - pre : 0) : -1;
        if (n < 0) { snprintf(out, cap, "%s", j); return; }
        size_t o = pre + n;
        if (o && out[o - 1] != '\n' && o + 1 < cap) out[o++] = '\n';
        snprintf(out + o, o < cap ? cap - o : 0, "```");
        return;
    }
    if (g == G_VERDICT) {
        const char *vp = strstr(j, "\"verdict\"");
        if (vp && json_str_after(vp, a, sizeof a) >= 0 && !strcmp(a, "FIX")) {
            const char *fp = strstr(j, "\"fix\"");
            if (fp && json_str_after(fp, b, sizeof b) >= 0) { snprintf(out, cap, "FIX: %s", b); return; }
        }
        snprintf(out, cap, "FACT");   // FACT means "the program output is the fact", used verbatim
        return;
    }
    if (g == G_OBSERVE) {
        const char *kp = strstr(j, "\"kind\""), *tp = strstr(j, "\"text\"");
        if (kp && tp && json_str_after(kp, a, sizeof a) >= 0 && json_str_after(tp, b, sizeof b) >= 0) {
            snprintf(out, cap, "%s: %s", strcmp(a, "FIX") ? "FACT" : "FIX", b);
            return;
        }
        snprintf(out, cap, "%s", j);
        return;
    }
    if (g == G_YESNO) {
        const char *ap = strstr(j, "\"answer\"");
        if (ap && json_str_after(ap, a, sizeof a) >= 0) { snprintf(out, cap, "%s", a); return; }
        snprintf(out, cap, "%s", j);
        return;
    }
    snprintf(out, cap, "%s", j);
}

int http_gen(http_engine *h, const char *gbnf, float temp, unsigned seed, int max_tok, char *out, unsigned long cap) {
    (void)seed; (void)max_tok;
    sb s = {0};
    int bad = 0;
    bad |= sb_raw(&s, "{\"model\":\""); bad |= sb_json(&s, h->model); bad |= sb_raw(&s, "\",\"stream\":false,\"messages\":[");
    for (int i = 0; i < h->n; i++) {
        if (i) bad |= sb_raw(&s, ",");
        bad |= sb_raw(&s, "{\"role\":\""); bad |= sb_raw(&s, h->role[i]);
        bad |= sb_raw(&s, "\",\"content\":\""); bad |= sb_json(&s, h->text[i]); bad |= sb_raw(&s, "\"}");
    }
    bad |= sb_raw(&s, "]");
    // schema mode: ollama/openai constrain output to a JSON schema; we reformat the reply back to the line format.
    const char *schema = (gbnf && (h->flavor == F_OLLAMA || h->flavor == F_OPENAI) && !schema_off()) ? schema_for(gbnf) : NULL;
    char tb[64];
    if (h->flavor == F_OLLAMA) {   // ollama takes temperature under options; a schema goes in "format"
        snprintf(tb, sizeof tb, ",\"options\":{\"temperature\":%.2f}", temp); bad |= sb_raw(&s, tb);
        if (schema) { bad |= sb_raw(&s, ",\"format\":"); bad |= sb_raw(&s, schema); }
    } else {
        snprintf(tb, sizeof tb, ",\"temperature\":%.2f", temp); bad |= sb_raw(&s, tb);
        if (gbnf && h->flavor == F_LLAMACPP) { bad |= sb_raw(&s, ",\"grammar\":\""); bad |= sb_json(&s, gbnf); bad |= sb_raw(&s, "\""); }
        if (gbnf && h->flavor == F_VLLM)     { bad |= sb_raw(&s, ",\"guided_grammar\":\""); bad |= sb_json(&s, gbnf); bad |= sb_raw(&s, "\""); }
        if (schema) { bad |= sb_raw(&s, ",\"response_format\":{\"type\":\"json_schema\",\"json_schema\":{\"name\":\"reply\",\"strict\":true,\"schema\":"); bad |= sb_raw(&s, schema); bad |= sb_raw(&s, "}}"); }
    }
    bad |= sb_raw(&s, "}");
    if (bad || !s.p) { free(s.p); out[0] = 0; return -1; }

    char url[640];
    snprintf(url, sizeof url, "%s%s", h->base, h->flavor == F_OLLAMA ? "/api/chat" : "/v1/chat/completions");
    char *resp = malloc(1 << 16);
    if (!resp) { free(s.p); out[0] = 0; return -1; }
    int (*req)(const char *, const char *, const char *, const char *, char *, unsigned long) = strncmp(url, "https://", 8) ? jb_http : jb_https;
    int code = req("POST", url, s.p, "application/json", resp, 1 << 16);
    free(s.p);
    if (code != 200) { if (getenv("JB_DEBUG_HTTP")) fprintf(stderr, "[http] %d %.400s\n", code, resp); free(resp); out[0] = 0; return -1; }

    // ollama: {...,"message":{"role":"assistant","content":"..."}}. openai: {...,"choices":[{..."message":{..."content":"..."}}]}.
    char *m = strstr(resp, "\"message\"");
    char *cptr = strstr(m ? m : resp, "\"content\"");
    if (!cptr) { if (getenv("JB_DEBUG_HTTP")) fprintf(stderr, "[http] no content in %.400s\n", resp); free(resp); out[0] = 0; return -1; }
    char *raw = malloc(1 << 16);
    int r = raw ? json_str_after(cptr, raw, 1 << 16) : -1;
    free(resp);
    if (r < 0) { free(raw); out[0] = 0; return -1; }
    if (schema) reformat(gbnf, raw, h->pending, sizeof h->pending);   // json -> the line format parse.c reads
    else snprintf(h->pending, sizeof h->pending, "%s", raw);
    free(raw);
    h->last_gen = words(h->pending);
    snprintf(out, cap, "%s", h->pending);
    return (int)strlen(out);
}
