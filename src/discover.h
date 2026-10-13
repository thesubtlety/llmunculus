#pragma once
// find GGUF models other tools already downloaded: ollama, LM Studio, the Hugging Face cache, llama.cpp, GPT4All, Jan.
typedef struct { char path[1024]; char name[96]; double gb; int score; } model_cand;
int discover_models(model_cand *out, int cap);   // best first. returns the count
const char *discover_best(void);                 // the cached pick, else a scan. NULL if nothing usable
const char *discover_cache_path(void);
