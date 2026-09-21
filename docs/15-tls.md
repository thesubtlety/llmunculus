# 15 tls

Status: done. HTTPS is native, via mbedtls compiled into the binary. No curl.

## why

The cosmocc toolchain ships no TLS, so HTTPS reports and probes had shelled out to curl. That needs curl on the host and is an external dependency for something core. Vendoring mbedtls removes it and gives every program a real HTTPS client.

## what was added

`third_party/mbedtls` (3.6.2) builds to `build/mbedtls/libmbedtls.a` for both arches, the same way tcc and llama.cpp do: list the sources, compile twice, archive. 108 files, about 1.9 MB, linked into `jb`. `mk/mbedtls.mk`.

`src/tls.c` is one function:

```c
int jb_https(const char *method, const char *url, const char *body, const char *ctype, char *out, unsigned long cap);
```

It parses `https://host[:port]/path`, connects, does the TLS handshake with SNI, sends an HTTP/1.0 request with `Connection: close`, reads the response to EOF, and returns the HTTP status with the body in `out`. One shot, no state.

It is registered in the symbol table and declared in `jb.h`, so a model program can call it like any helper. `report_send` uses it for `https://` destinations; `http://` still uses the native plaintext client from the report work. curl is gone from both paths.

## certificate verification

On connect it looks for a CA bundle at the usual places: `/etc/ssl/certs/ca-certificates.crt`, `/etc/pki/tls/certs/ca-bundle.crt`, `/etc/ssl/cert.pem`, and one more. If one loads, the certificate is verified and a bad cert fails the request. If none is found, it proceeds unverified, the way `curl -k` does, which is the common case for an internal endpoint with a private CA. `JB_TLS_INSECURE=1` forces unverified even when a bundle exists.

## proof

`jb_https GET https://example.com/` returned 200 with the page, cert verified against this box's bundle. A report POST to a real https endpoint completed the handshake and returned the server's status (405 there, since it does not accept the POST). Both without curl.

## what this does not do

- Only HTTPS. SSH is still the `ssh` binary: SSH is a different protocol, not TLS, and would be a separate port. See 14-remote.md.
- HTTP/1.0, no chunked, no redirects, no keep-alive. It is a reporting and health-check client, not a browser. A redirect returns the 3xx for the caller to handle.
