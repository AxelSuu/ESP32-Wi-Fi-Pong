#include "proto.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Substring-based field finder. SAFE ONLY for a CLOSED schema with distinct
// keys: strstr matches "key" anywhere, so a key that is a substring of another
// key, or a value string equal to a key name, can mis-match. The controller's
// schema keeps keys short and unique on purpose — do not feed untrusted JSON.
// Returns a pointer just past the colon (spaces skipped), or NULL.
static const char *json_value(const char *msg, const char *key)
{
    char pat[20];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(msg, pat);
    if (!p) return NULL;
    p = strchr(p + strlen(pat), ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

bool proto_find_str(const char *msg, const char *key, char *out, size_t cap)
{
    const char *p = json_value(msg, key);
    if (!p || *p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < cap - 1) out[i++] = *p++;
    out[i] = '\0';
    return true;
}

bool proto_find_num(const char *msg, const char *key, double *out)
{
    const char *p = json_value(msg, key);
    if (!p) return false;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return false;
    *out = v;
    return true;
}

int proto_fmt_welcome(char *buf, size_t cap, int player)
{
    return snprintf(buf, cap, "{\"v\":%d,\"t\":\"welcome\",\"player\":%d}",
                    PROTO_SCHEMA_VERSION, player);
}

int proto_fmt_system_info(char *buf, size_t cap, const char *version)
{
    return snprintf(buf, cap, "{\"v\":%d,\"t\":\"system_info\",\"version\":\"%s\"}",
                    PROTO_SCHEMA_VERSION, version);
}

int proto_fmt_active(char *buf, size_t cap, const char *game_id, int players)
{
    return snprintf(buf, cap, "{\"v\":%d,\"t\":\"active\",\"game\":\"%s\",\"players\":%d}",
                    PROTO_SCHEMA_VERSION, game_id, players);
}

int proto_fmt_waiting(char *buf, size_t cap, int need, int have)
{
    return snprintf(buf, cap, "{\"v\":%d,\"t\":\"waiting\",\"need\":%d,\"have\":%d}",
                    PROTO_SCHEMA_VERSION, need, have);
}

int proto_fmt_over(char *buf, size_t cap, int winner, int score)
{
    return snprintf(buf, cap, "{\"v\":%d,\"t\":\"over\",\"winner\":%d,\"score\":%d}",
                    PROTO_SCHEMA_VERSION, winner, score);
}

int proto_fmt_pong(char *buf, size_t cap, double ts)
{
    return snprintf(buf, cap, "{\"v\":%d,\"t\":\"pong\",\"ts\":%.0f}",
                    PROTO_SCHEMA_VERSION, ts);
}
