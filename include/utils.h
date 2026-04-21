/*
 * utils.h — Shared utility functions for mycontainer
 *
 * Provides file I/O, lightweight JSON parsing/writing,
 * string helpers, directory operations, and ID generation.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

/* ---- Constants ---- */
#define MAX_JSON_KEYS      64
#define MAX_JSON_ARRAY     128
#define MAX_PATH_LEN       1024
#define MAX_CMD_LEN        2048
#define MAX_BUF            4096
#define MAX_LINE           1024
#define ID_LEN             8

/* ---- JSON Key-Value Pair ---- */
typedef struct {
    char key[64];
    char value[2048];
    int  is_string;  /* 1 = always quote, 0 = auto-detect (arrays/objects/bool/null) */
} JsonPair;

/* ---- JSON Object (flat key-value map) ---- */
typedef struct {
    JsonPair pairs[MAX_JSON_KEYS];
    int      count;
} JsonObject;

/* ---- JSON Array of Objects ---- */
typedef struct {
    JsonObject *objects;   /* heap-allocated array */
    int         count;
    int         capacity;
} JsonArray;

/* Allocate a new JsonArray on the heap. Caller must call json_array_free(). */
JsonArray *json_array_new(void);

/* Free a heap-allocated JsonArray. */
void json_array_free(JsonArray *arr);

/* ---- File I/O ---- */

/* Read entire file into malloc'd buffer. Caller must free(). Returns NULL on error. */
char *read_file(const char *path);

/* Write string to file (overwrites). Returns 0 on success, -1 on error. */
int write_file(const char *path, const char *content);

/* Append string to file. Returns 0 on success, -1 on error. */
int append_file(const char *path, const char *content);

/* Check if file exists. Returns 1 if yes, 0 if no. */
int file_exists(const char *path);

/* Check if directory exists. Returns 1 if yes, 0 if no. */
int dir_exists(const char *path);

/* Copy a file from src to dest. Returns 0 on success. */
int copy_file(const char *src, const char *dest);

/* Get file size in bytes. Returns -1 on error. */
long get_file_size(const char *path);

/* Format file size as human-readable string (e.g. "5MB"). */
void format_size(long bytes, char *out, size_t out_len);

/* Format a string into a buffer and fail cleanly if it would overflow. */
int format_buffer(char *out, size_t out_len, const char *fmt, ...);

/* ---- Directory Operations ---- */

/* Create directory and all parents (like mkdir -p). Returns 0 on success. */
int mkdir_p(const char *path);

/* Recursively remove a directory and all contents. Returns 0 on success. */
int rmdir_recursive(const char *path);

/* ---- JSON Parsing ---- */

/* Parse a JSON object string into a JsonObject. Returns 0 on success. */
int json_parse(const char *json_str, JsonObject *obj);

/* Get value for a key from a JsonObject. Returns NULL if not found. */
const char *json_get(const JsonObject *obj, const char *key);

/* Set a key-value pair in a JsonObject (adds or updates). Always quoted as string. */
void json_set(JsonObject *obj, const char *key, const char *value);

/* Set a raw (unquoted) value — use for numbers, booleans, null, arrays, objects. */
void json_set_raw(JsonObject *obj, const char *key, const char *value);

/* Serialize a JsonObject to a JSON string. Caller must free(). */
char *json_stringify(const JsonObject *obj);

/* Serialize a JsonObject with pretty printing (indented). Caller must free(). */
char *json_stringify_pretty(const JsonObject *obj);

/* Free any internal allocations in a JsonObject (currently a no-op for stack arrays). */
void json_free(JsonObject *obj);

/* ---- JSON Array Operations ---- */

/* Parse a JSON array of objects. Returns 0 on success. */
int json_parse_array(const char *json_str, JsonArray *arr);

/* Parse a JSON array of strings into fixed buffers. Returns number parsed or -1. */
int json_parse_string_array(const char *json_str, char values[][MAX_LINE], int max_values);

/* Append an object to a JsonArray. */
void json_array_append(JsonArray *arr, const JsonObject *obj);

/* Remove object at index from a JsonArray. */
void json_array_remove(JsonArray *arr, int index);

/* Serialize a JsonArray to a JSON string. Caller must free(). */
char *json_array_stringify(const JsonArray *arr);

/* Serialize a JsonArray with pretty printing. Caller must free(). */
char *json_array_stringify_pretty(const JsonArray *arr);

/* Serialize a plain list of strings as a JSON array. Caller must free(). */
char *json_string_array_from_list(char *values[], int count);

/* ---- String Helpers ---- */

/* Generate a random hex ID (8 chars). Writes to out (must be >= ID_LEN+1). */
void generate_id(char *out);

/* Get current ISO 8601 timestamp. Writes to out (must be >= 32 chars). */
void get_timestamp(char *out, size_t out_len);

/* Split "name:tag" into separate name and tag strings. Returns 0 on success. */
int split_name_tag(const char *input, char *name, size_t name_len,
                   char *tag, size_t tag_len);

/* Trim leading and trailing whitespace in-place. */
void str_trim(char *str);

/* Portable strsep-style splitter for delimiter-separated strings. */
char *strsep_local(char **stringp, const char *delim);

/* Check if string starts with prefix. */
int str_starts_with(const char *str, const char *prefix);

/* Compute a human-readable "time ago" string from ISO timestamp. */
void time_ago(const char *iso_timestamp, char *out, size_t out_len);

/* ---- CLI Helpers ---- */

/* Check if --json flag is present in argv. */
int has_json_flag(int argc, char *argv[]);

/* Check if a --key=value flag is present and return value. Returns NULL if not found. */
const char *get_flag_value(int argc, char *argv[], const char *flag);

#endif /* UTILS_H */
