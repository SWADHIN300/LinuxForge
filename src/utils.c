/*
 * utils.c — Shared Utility Library for mycontainer
 *
 * PURPOSE:
 *   Provide all the cross-cutting infrastructure that the rest of the
 *   engine depends on: file I/O, a purpose-built JSON engine, directory
 *   management, string helpers, ID generation, and CLI flag parsing.
 *
 * SECTIONS:
 *
 *   1. FILE I/O
 *      read_file()       — malloc + slurp entire file into a C string
 *      write_file()      — atomic overwrite of file with content string
 *      append_file()     — append string to end of file
 *      file_exists()     — stat(2)-based regular file existence check
 *      dir_exists()      — stat(2)-based directory existence check
 *      copy_file()       — buffered binary file copy (8 KB chunks)
 *      get_file_size()   — return file size in bytes via stat(2)
 *      format_size()     — format bytes as "1.2GB"/"512MB"/"4KB" etc.
 *      format_buffer()   — safe snprintf wrapper; returns -1 on truncation
 *
 *   2. DIRECTORY OPERATIONS
 *      mkdir_p()         — create full directory path (like mkdir -p)
 *      rmdir_recursive() — recursively remove directory + contents
 *
 *   3. JSON ENGINE (purpose-built, no external dependencies)
 *      Handles flat JSON objects and arrays of flat objects.
 *      Values are stored internally as strings; is_string=1 means the
 *      value will be JSON-quoted; is_string=0 means raw (numbers/arrays).
 *
 *      Parsing:
 *        json_parse()             — JSON object string → JsonObject
 *        json_parse_array()       — JSON array string → JsonArray
 *        json_parse_string_array()— JSON string array → char[][MAX_LINE]
 *
 *      Query / Mutation:
 *        json_get()       — look up value by key (returns C string or NULL)
 *        json_set()       — set quoted string key-value pair
 *        json_set_raw()   — set unquoted (numeric/array/object) key-value
 *
 *      Serialisation:
 *        json_stringify()        — compact one-line JSON object
 *        json_stringify_pretty() — indented multi-line JSON object
 *        json_array_stringify()  — compact array of objects
 *        json_array_stringify_pretty() — indented array of objects
 *        json_string_array_from_list() — char*[] → JSON string array
 *
 *      Array lifecycle:
 *        json_array_new()    — heap-allocate a JsonArray (capacity MAX_JSON_ARRAY)
 *        json_array_free()   — release the array and its object storage
 *        json_array_append() — copy a JsonObject into the array
 *        json_array_remove() — remove entry at index (shifts remaining left)
 *
 *   4. STRING / MISC HELPERS
 *      generate_id()       — generate a random 12-char lowercase hex ID
 *      get_timestamp()     — current UTC time as ISO 8601 string
 *      split_name_tag()    — split "name:tag" → separate name + tag buffers
 *      str_trim()          — in-place strip leading/trailing whitespace
 *      strsep_local()      — strsep(3) replacement for platforms missing it
 *      has_json_flag()     — scan argv[] for "--json" flag
 *      get_flag_value()    — get value of "--flag=value" from argv[]
 *
 * CROSS-PLATFORM NOTES:
 *   Windows (_WIN32): uses _mkdir() instead of mkdir(2).
 *   _GNU_SOURCE is required for strsep(3) on glibc.
 */

#define _GNU_SOURCE
#include "../include/utils.h"
#include <ctype.h>
#include <fcntl.h>

#ifdef _WIN32
#include <direct.h>
#endif

/* ================================================================
 * FILE I/O
 * ================================================================ */

char *read_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0) { fclose(fp); return NULL; }

    char *buf = malloc(size + 1);
    if (!buf) { fclose(fp); return NULL; }

    size_t read = fread(buf, 1, size, fp);
    buf[read] = '\0';
    fclose(fp);
    return buf;
}

int write_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: cannot write to %s: %s\n", path, strerror(errno));
        return -1;
    }
    fputs(content, fp);
    fclose(fp);
    return 0;
}

int append_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "Error: cannot append to %s: %s\n", path, strerror(errno));
        return -1;
    }
    fputs(content, fp);
    fclose(fp);
    return 0;
}

int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

int dir_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int copy_file(const char *src, const char *dest) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        fprintf(stderr, "Error: cannot open source %s: %s\n", src, strerror(errno));
        return -1;
    }

    FILE *out = fopen(dest, "wb");
    if (!out) {
        fprintf(stderr, "Error: cannot open dest %s: %s\n", dest, strerror(errno));
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fprintf(stderr, "Error: write failed to %s\n", dest);
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}

void format_size(long bytes, char *out, size_t out_len) {
    if (bytes < 0) {
        snprintf(out, out_len, "N/A");
    } else if (bytes < 1024) {
        snprintf(out, out_len, "%ldB", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(out, out_len, "%ldKB", bytes / 1024);
    } else if (bytes < 1024L * 1024 * 1024) {
        snprintf(out, out_len, "%ldMB", bytes / (1024 * 1024));
    } else {
        snprintf(out, out_len, "%.1fGB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

int format_buffer(char *out, size_t out_len, const char *fmt, ...) {
    va_list args;
    int written;

    if (!out || out_len == 0 || !fmt) {
        errno = EINVAL;
        return -1;
    }

    va_start(args, fmt);
    written = vsnprintf(out, out_len, fmt, args);
    va_end(args);

    if (written < 0 || (size_t) written >= out_len) {
        out[0] = '\0';
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

/* ================================================================
 * DIRECTORY OPERATIONS
 * ================================================================ */

static int mkdir_single(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

int mkdir_p(const char *path) {
    char tmp[MAX_PATH_LEN];
    char *p = NULL;

    if (format_buffer(tmp, sizeof(tmp), "%s", path) != 0) {
        fprintf(stderr, "Error: path too long: %s\n", path);
        return -1;
    }

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!dir_exists(tmp)) {
                if (mkdir_single(tmp) != 0 && errno != EEXIST) {
                    fprintf(stderr, "Error: mkdir %s: %s\n", tmp, strerror(errno));
                    return -1;
                }
            }
            *p = '/';
        }
    }
    if (!dir_exists(tmp)) {
        if (mkdir_single(tmp) != 0 && errno != EEXIST) {
            fprintf(stderr, "Error: mkdir %s: %s\n", tmp, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int rmdir_recursive(const char *path) {
    struct stat st;

    if (!path || !*path) {
        errno = EINVAL;
        return -1;
    }

    if (stat(path, &st) != 0) {
        return (errno == ENOENT) ? 0 : -1;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *entry;

        if (!dir) {
            fprintf(stderr, "Error: cannot open %s: %s\n", path, strerror(errno));
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            char child[MAX_PATH_LEN];

            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            if (format_buffer(child, sizeof(child), "%s/%s", path, entry->d_name) != 0) {
                fprintf(stderr, "Error: path too long under %s\n", path);
                closedir(dir);
                return -1;
            }

            if (rmdir_recursive(child) != 0) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);
        if (rmdir(path) != 0) {
            fprintf(stderr, "Error: rmdir %s: %s\n", path, strerror(errno));
            return -1;
        }
        return 0;
    }

    if (remove(path) != 0) {
        fprintf(stderr, "Error: remove %s: %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

/* ================================================================
 * JSON PARSING (lightweight, purpose-built)
 *
 * Handles flat JSON objects and arrays of flat objects.
 * Values are always stored as strings internally.
 * ================================================================ */

/* Skip whitespace */
static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Parse a JSON string (expects p pointing at opening quote).
 * Writes the string content to out (without quotes).
 * Returns pointer after closing quote, or NULL on error. */
static const char *parse_json_string(const char *p, char *out, size_t out_len) {
    if (*p != '"') return NULL;
    p++;

    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case '"':  if (i < out_len - 1) out[i++] = '"';  break;
                case '\\': if (i < out_len - 1) out[i++] = '\\'; break;
                case 'n':  if (i < out_len - 1) out[i++] = '\n'; break;
                case 't':  if (i < out_len - 1) out[i++] = '\t'; break;
                case '/':  if (i < out_len - 1) out[i++] = '/';  break;
                default:   if (i < out_len - 1) out[i++] = *p;   break;
            }
        } else {
            if (i < out_len - 1) out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

static int append_json_escaped(char *buf, size_t buf_size, int *pos, const char *value) {
    if (!buf || !pos || !value) return -1;

    for (const unsigned char *p = (const unsigned char *) value; *p; p++) {
        const char *escape = NULL;
        char literal[7];

        switch (*p) {
            case '"':  escape = "\\\""; break;
            case '\\': escape = "\\\\"; break;
            case '\n': escape = "\\n"; break;
            case '\r': escape = "\\r"; break;
            case '\t': escape = "\\t"; break;
            case '\b': escape = "\\b"; break;
            case '\f': escape = "\\f"; break;
            default:
                if (*p < 0x20) {
                    snprintf(literal, sizeof(literal), "\\u%04x", *p);
                    escape = literal;
                }
                break;
        }

        if (escape) {
            int written = snprintf(buf + *pos, buf_size - *pos, "%s", escape);
            if (written < 0 || (size_t) written >= buf_size - *pos) return -1;
            *pos += written;
        } else {
            if ((size_t) *pos + 1 >= buf_size) return -1;
            buf[*pos] = (char) *p;
            (*pos)++;
            buf[*pos] = '\0';
        }
    }

    return 0;
}

/* Parse a JSON value — could be string, number, boolean, null, or array.
 * For non-string types, stores the raw text representation.
 * For arrays, stores the entire array as a string "[...]". */
static const char *parse_json_value(const char *p, char *out, size_t out_len) {
    p = skip_ws(p);

    if (*p == '"') {
        return parse_json_string(p, out, out_len);
    }

    /* Array — capture entire array as string */
    if (*p == '[') {
        size_t i = 0;
        int depth = 0;
        do {
            if (*p == '[') depth++;
            if (*p == ']') depth--;
            if (i < out_len - 1) out[i++] = *p;
            p++;
        } while (*p && depth > 0);
        out[i] = '\0';
        return p;
    }

    /* Nested object — capture entire object as string */
    if (*p == '{') {
        size_t i = 0;
        int depth = 0;
        do {
            if (*p == '{') depth++;
            if (*p == '}') depth--;
            if (i < out_len - 1) out[i++] = *p;
            p++;
        } while (*p && depth > 0);
        out[i] = '\0';
        return p;
    }

    /* Number, boolean, null */
    size_t i = 0;
    while (*p && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p)) {
        if (i < out_len - 1) out[i++] = *p;
        p++;
    }
    out[i] = '\0';
    return p;
}

int json_parse(const char *json_str, JsonObject *obj) {
    if (!json_str || !obj) return -1;

    obj->count = 0;
    const char *p = skip_ws(json_str);

    if (*p != '{') return -1;
    p++;

    while (*p && *p != '}') {
        p = skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }

        /* Parse key */
        if (obj->count >= MAX_JSON_KEYS) break;
        JsonPair *pair = &obj->pairs[obj->count];
        memset(pair, 0, sizeof(JsonPair));

        p = parse_json_string(p, pair->key, sizeof(pair->key));
        if (!p) return -1;

        p = skip_ws(p);
        if (*p != ':') return -1;
        p++;

        /* Detect if value is a quoted string */
        const char *val_start = skip_ws(p);
        int was_string = (*val_start == '"') ? 1 : 0;

        /* Parse value */
        p = parse_json_value(p, pair->value, sizeof(pair->value));
        if (!p) return -1;

        pair->is_string = was_string;

        obj->count++;
        p = skip_ws(p);
    }
    return 0;
}

const char *json_get(const JsonObject *obj, const char *key) {
    if (!obj || !key) return NULL;
    for (int i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0)
            return obj->pairs[i].value;
    }
    return NULL;
}

/* Internal helper for setting key-value with type flag */
static void json_set_internal(JsonObject *obj, const char *key,
                              const char *value, int is_string) {
    if (!obj || !key || !value) return;

    /* Update existing key */
    for (int i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) {
            strncpy(obj->pairs[i].value, value, sizeof(obj->pairs[i].value) - 1);
            obj->pairs[i].value[sizeof(obj->pairs[i].value) - 1] = '\0';
            obj->pairs[i].is_string = is_string;
            return;
        }
    }

    /* Add new key */
    if (obj->count < MAX_JSON_KEYS) {
        JsonPair *p = &obj->pairs[obj->count];
        strncpy(p->key, key, sizeof(p->key) - 1);
        p->key[sizeof(p->key) - 1] = '\0';
        strncpy(p->value, value, sizeof(p->value) - 1);
        p->value[sizeof(p->value) - 1] = '\0';
        p->is_string = is_string;
        obj->count++;
    }
}

void json_set(JsonObject *obj, const char *key, const char *value) {
    json_set_internal(obj, key, value, 1); /* always quoted */
}

void json_set_raw(JsonObject *obj, const char *key, const char *value) {
    json_set_internal(obj, key, value, 0); /* unquoted */
}

char *json_stringify(const JsonObject *obj) {
    if (!obj) return NULL;

    char *buf = malloc(MAX_BUF * 4);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, MAX_BUF * 4 - pos, "{");

    for (int i = 0; i < obj->count; i++) {
        if (i > 0) pos += snprintf(buf + pos, MAX_BUF * 4 - pos, ",");

        const char *v = obj->pairs[i].value;
        int quote = obj->pairs[i].is_string;

        if (quote) {
            pos += snprintf(buf + pos, MAX_BUF * 4 - pos,
                            "\"%s\":\"", obj->pairs[i].key);
            if (pos < 0 || pos >= MAX_BUF * 4 ||
                append_json_escaped(buf, MAX_BUF * 4, &pos, v) != 0) {
                free(buf);
                return NULL;
            }
            pos += snprintf(buf + pos, MAX_BUF * 4 - pos, "\"");
        } else {
            pos += snprintf(buf + pos, MAX_BUF * 4 - pos,
                            "\"%s\":%s", obj->pairs[i].key, v);
        }
    }

    pos += snprintf(buf + pos, MAX_BUF * 4 - pos, "}");
    return buf;
}

char *json_stringify_pretty(const JsonObject *obj) {
    if (!obj) return NULL;

    char *buf = malloc(MAX_BUF * 4);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, MAX_BUF * 4 - pos, "{\n");

    for (int i = 0; i < obj->count; i++) {
        const char *v = obj->pairs[i].value;
        int quote = obj->pairs[i].is_string;

        if (quote) {
            pos += snprintf(buf + pos, MAX_BUF * 4 - pos,
                            "  \"%s\": \"", obj->pairs[i].key);
            if (pos < 0 || pos >= MAX_BUF * 4 ||
                append_json_escaped(buf, MAX_BUF * 4, &pos, v) != 0) {
                free(buf);
                return NULL;
            }
            pos += snprintf(buf + pos, MAX_BUF * 4 - pos, "\"");
        } else {
            pos += snprintf(buf + pos, MAX_BUF * 4 - pos,
                            "  \"%s\": %s", obj->pairs[i].key, v);
        }

        if (i < obj->count - 1)
            pos += snprintf(buf + pos, MAX_BUF * 4 - pos, ",\n");
        else
            pos += snprintf(buf + pos, MAX_BUF * 4 - pos, "\n");
    }

    pos += snprintf(buf + pos, MAX_BUF * 4 - pos, "}");
    return buf;
}

void json_free(JsonObject *obj) {
    if (obj) obj->count = 0;
}

/* ================================================================
 * JSON ARRAY OPERATIONS (heap-allocated)
 * ================================================================ */

JsonArray *json_array_new(void) {
    JsonArray *arr = malloc(sizeof(JsonArray));
    if (!arr) return NULL;
    arr->capacity = MAX_JSON_ARRAY;
    arr->count = 0;
    arr->objects = calloc(arr->capacity, sizeof(JsonObject));
    if (!arr->objects) {
        free(arr);
        return NULL;
    }
    return arr;
}

void json_array_free(JsonArray *arr) {
    if (!arr) return;
    free(arr->objects);
    arr->objects = NULL;
    arr->count = 0;
    arr->capacity = 0;
    free(arr);
}

int json_parse_array(const char *json_str, JsonArray *arr) {
    if (!json_str || !arr || !arr->objects) return -1;
    arr->count = 0;

    const char *p = skip_ws(json_str);
    if (*p != '[') return -1;
    p++;

    while (*p && *p != ']') {
        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') { p++; continue; }

        if (*p == '{') {
            /* Find the end of this object */
            const char *start = p;
            int depth = 0;
            const char *end = p;
            do {
                if (*end == '{') depth++;
                if (*end == '}') depth--;
                end++;
            } while (*end && depth > 0);

            /* Extract the object substring */
            size_t len = end - start;
            char *obj_str = malloc(len + 1);
            if (!obj_str) return -1;
            memcpy(obj_str, start, len);
            obj_str[len] = '\0';

            if (arr->count < arr->capacity) {
                json_parse(obj_str, &arr->objects[arr->count]);
                arr->count++;
            }

            free(obj_str);
            p = end;
        } else {
            p++;
        }
    }

    return 0;
}

int json_parse_string_array(const char *json_str, char values[][MAX_LINE], int max_values) {
    const char *p;
    int count = 0;

    if (!json_str || !values || max_values <= 0) return -1;

    p = skip_ws(json_str);
    if (*p != '[') return -1;
    p++;

    while (*p && *p != ']') {
        char item[MAX_LINE];

        p = skip_ws(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == ']') break;
        if (*p != '"') return -1;

        p = parse_json_string(p, item, sizeof(item));
        if (!p) return -1;

        if (count < max_values) {
            strncpy(values[count], item, MAX_LINE - 1);
            values[count][MAX_LINE - 1] = '\0';
            count++;
        }

        p = skip_ws(p);
        if (*p == ',') p++;
    }

    return count;
}

void json_array_append(JsonArray *arr, const JsonObject *obj) {
    if (!arr || !obj || !arr->objects || arr->count >= arr->capacity) return;
    memcpy(&arr->objects[arr->count], obj, sizeof(JsonObject));
    arr->count++;
}

void json_array_remove(JsonArray *arr, int index) {
    if (!arr || !arr->objects || index < 0 || index >= arr->count) return;
    for (int i = index; i < arr->count - 1; i++) {
        memcpy(&arr->objects[i], &arr->objects[i + 1], sizeof(JsonObject));
    }
    arr->count--;
}

char *json_array_stringify(const JsonArray *arr) {
    if (!arr || !arr->objects) return NULL;

    /* Estimate needed size */
    size_t buf_size = MAX_BUF * 8;
    char *buf = malloc(buf_size);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, buf_size - pos, "[");

    for (int i = 0; i < arr->count; i++) {
        if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
        char *obj_str = json_stringify(&arr->objects[i]);
        if (obj_str) {
            pos += snprintf(buf + pos, buf_size - pos, "%s", obj_str);
            free(obj_str);
        }
    }

    pos += snprintf(buf + pos, buf_size - pos, "]");
    return buf;
}

char *json_array_stringify_pretty(const JsonArray *arr) {
    if (!arr || !arr->objects) return NULL;

    size_t buf_size = MAX_BUF * 16;
    char *buf = malloc(buf_size);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, buf_size - pos, "[\n");

    for (int i = 0; i < arr->count; i++) {
        if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",\n");

        /* Indent the object */
        pos += snprintf(buf + pos, buf_size - pos, "  {\n");
        const JsonObject *obj = &arr->objects[i];
        for (int j = 0; j < obj->count; j++) {
            const char *v = obj->pairs[j].value;
            int quote = obj->pairs[j].is_string;

            if (quote) {
                pos += snprintf(buf + pos, buf_size - pos,
                                "    \"%s\": \"", obj->pairs[j].key);
                if (pos < 0 || (size_t) pos >= buf_size ||
                    append_json_escaped(buf, buf_size, &pos, v) != 0) {
                    free(buf);
                    return NULL;
                }
                pos += snprintf(buf + pos, buf_size - pos, "\"");
            } else {
                pos += snprintf(buf + pos, buf_size - pos,
                                "    \"%s\": %s", obj->pairs[j].key, v);
            }

            if (j < obj->count - 1)
                pos += snprintf(buf + pos, buf_size - pos, ",\n");
            else
                pos += snprintf(buf + pos, buf_size - pos, "\n");
        }
        pos += snprintf(buf + pos, buf_size - pos, "  }");
    }

    pos += snprintf(buf + pos, buf_size - pos, "\n]");
    return buf;
}

char *json_string_array_from_list(char *values[], int count) {
    size_t buf_size;
    char *buf;
    int pos = 0;

    if (count < 0) return NULL;

    buf_size = (size_t) (MAX_BUF * 4);
    buf = malloc(buf_size);
    if (!buf) return NULL;

    pos += snprintf(buf + pos, buf_size - pos, "[");

    for (int i = 0; i < count; i++) {
        const char *value = values[i] ? values[i] : "";

        if (i > 0) {
            pos += snprintf(buf + pos, buf_size - pos, ", ");
        }

        pos += snprintf(buf + pos, buf_size - pos, "\"");
        if (pos < 0 || (size_t) pos >= buf_size ||
            append_json_escaped(buf, buf_size, &pos, value) != 0) {
            free(buf);
            return NULL;
        }
        pos += snprintf(buf + pos, buf_size - pos, "\"");
    }

    pos += snprintf(buf + pos, buf_size - pos, "]");
    return buf;
}

/* ================================================================
 * STRING HELPERS
 * ================================================================ */

void generate_id(char *out) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)(time(NULL) ^ getpid()));
        seeded = 1;
    }
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < ID_LEN; i++) {
        out[i] = hex[rand() % 16];
    }
    out[ID_LEN] = '\0';
}

void get_timestamp(char *out, size_t out_len) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    strftime(out, out_len, "%Y-%m-%dT%H:%M:%SZ", tm);
}

int split_name_tag(const char *input, char *name, size_t name_len,
                   char *tag, size_t tag_len) {
    if (!input || !name || !tag) return -1;

    const char *colon = strchr(input, ':');
    if (!colon) {
        /* No tag specified, use "latest" */
        strncpy(name, input, name_len - 1);
        name[name_len - 1] = '\0';
        strncpy(tag, "latest", tag_len - 1);
        tag[tag_len - 1] = '\0';
        return 0;
    }

    size_t nlen = colon - input;
    if (nlen >= name_len) nlen = name_len - 1;
    memcpy(name, input, nlen);
    name[nlen] = '\0';

    strncpy(tag, colon + 1, tag_len - 1);
    tag[tag_len - 1] = '\0';

    return 0;
}

void str_trim(char *str) {
    if (!str) return;

    /* Trim leading */
    char *start = str;
    while (isspace((unsigned char)*start)) start++;

    /* If string is now empty, just terminate at str[0] */
    if (*start == '\0') {
        str[0] = '\0';
        return;
    }

    /* Trim trailing */
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

char *strsep_local(char **stringp, const char *delim) {
    char *start;
    char *cursor;

    if (!stringp || !*stringp || !delim) return NULL;

    start = *stringp;
    cursor = start;

    while (*cursor) {
        if (strchr(delim, *cursor) != NULL) {
            *cursor = '\0';
            *stringp = cursor + 1;
            return start;
        }
        cursor++;
    }

    *stringp = NULL;
    return start;
}

int str_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

void time_ago(const char *iso_timestamp, char *out, size_t out_len) {
    if (!iso_timestamp || !out) return;

    struct tm tm = {0};
    /* Parse ISO 8601: "2024-01-15T10:30:00Z" */
    if (sscanf(iso_timestamp, "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) < 6) {
        snprintf(out, out_len, "unknown");
        return;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

    time_t then = mktime(&tm);
    time_t now = time(NULL);

    /* Adjust for UTC vs local */
    struct tm *gm = gmtime(&now);
    time_t now_utc = mktime(gm);

    double diff = difftime(now_utc, then);
    if (diff < 0) diff = 0;

    long secs = (long)diff;
    if (secs < 60) {
        snprintf(out, out_len, "%ld sec ago", secs);
    } else if (secs < 3600) {
        snprintf(out, out_len, "%ld min ago", secs / 60);
    } else if (secs < 86400) {
        long hrs = secs / 3600;
        snprintf(out, out_len, "%ld hour%s ago", hrs, hrs == 1 ? "" : "s");
    } else {
        long days = secs / 86400;
        snprintf(out, out_len, "%ld day%s ago", days, days == 1 ? "" : "s");
    }
}

/* ================================================================
 * CLI HELPERS
 * ================================================================ */

int has_json_flag(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) return 1;
    }
    return 0;
}

const char *get_flag_value(int argc, char *argv[], const char *flag) {
    size_t flen = strlen(flag);
    for (int i = 1; i < argc; i++) {
        /* --flag=value */
        if (strncmp(argv[i], flag, flen) == 0 && argv[i][flen] == '=') {
            return argv[i] + flen + 1;
        }
        /* --flag value */
        if (strcmp(argv[i], flag) == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return NULL;
}
