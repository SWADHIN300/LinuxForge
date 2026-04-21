/*
 * registry.c — Container image registry
 *
 * Manages a local image store: push, pull, list, delete, inspect.
 * Images are stored as rootfs.tar.gz + config.json in registry/images/.
 * An index file registry/images.json tracks all images.
 */

#define _GNU_SOURCE
#include "../include/registry.h"
#include <ctype.h>
#include <fcntl.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

/* ================================================================
 * REGISTRY INIT
 * ================================================================ */

int registry_init(void) {
    if (mkdir_p(REGISTRY_IMAGES_DIR) != 0) {
        fprintf(stderr, "Error: failed to create registry directories\n");
        return -1;
    }

    if (!file_exists(REGISTRY_INDEX)) {
        if (write_file(REGISTRY_INDEX, "[]") != 0) {
            fprintf(stderr, "Error: failed to create %s\n", REGISTRY_INDEX);
            return -1;
        }
    }

    return 0;
}

/* ================================================================
 * IMAGE <-> JSON HELPERS
 * ================================================================ */

static void image_to_json_object(const Image *img, JsonObject *obj) {
    obj->count = 0;
    json_set(obj, "id", img->id);
    json_set(obj, "name", img->name);
    json_set(obj, "tag", img->tag);
    json_set(obj, "size", img->size);

    char layers_str[16];
    snprintf(layers_str, sizeof(layers_str), "%d", img->layers);
    json_set_raw(obj, "layers", layers_str);

    json_set(obj, "created_at", img->created_at);
    if (strlen(img->runtime) > 0)
        json_set(obj, "runtime", img->runtime);
    if (strlen(img->cmd) > 0)
        json_set(obj, "cmd", img->cmd);
    if (strlen(img->digest) > 0)
        json_set(obj, "digest", img->digest);
    if (strlen(img->signature) > 0)
        json_set(obj, "signature", img->signature);
    if (strlen(img->signed_at) > 0)
        json_set(obj, "signed_at", img->signed_at);
    json_set(obj, "rootfs_path", img->rootfs_path);
    json_set(obj, "config_path", img->config_path);
}

static void json_object_to_image(const JsonObject *obj, Image *img) {
    memset(img, 0, sizeof(Image));

    const char *v;
    if ((v = json_get(obj, "id")))
        strncpy(img->id, v, sizeof(img->id) - 1);
    if ((v = json_get(obj, "name")))
        strncpy(img->name, v, sizeof(img->name) - 1);
    if ((v = json_get(obj, "tag")))
        strncpy(img->tag, v, sizeof(img->tag) - 1);
    if ((v = json_get(obj, "size")))
        strncpy(img->size, v, sizeof(img->size) - 1);
    if ((v = json_get(obj, "layers")))
        img->layers = atoi(v);
    if ((v = json_get(obj, "created_at")))
        strncpy(img->created_at, v, sizeof(img->created_at) - 1);
    if ((v = json_get(obj, "runtime")))
        strncpy(img->runtime, v, sizeof(img->runtime) - 1);
    if ((v = json_get(obj, "cmd")))
        strncpy(img->cmd, v, sizeof(img->cmd) - 1);
    if ((v = json_get(obj, "digest")))
        strncpy(img->digest, v, sizeof(img->digest) - 1);
    if ((v = json_get(obj, "signature")))
        strncpy(img->signature, v, sizeof(img->signature) - 1);
    if ((v = json_get(obj, "signed_at")))
        strncpy(img->signed_at, v, sizeof(img->signed_at) - 1);
    if ((v = json_get(obj, "rootfs_path")))
        strncpy(img->rootfs_path, v, sizeof(img->rootfs_path) - 1);
    if ((v = json_get(obj, "config_path")))
        strncpy(img->config_path, v, sizeof(img->config_path) - 1);
}

static void registry_normalize_image_object(JsonObject *obj) {
    static const char *string_keys[] = {
        "id", "name", "tag", "size", "created_at",
        "runtime", "cmd", "digest", "signature", "signed_at",
        "rootfs_path", "config_path", "committed_from",
        "base_image", "description", NULL
    };
    const char *value;

    if (!obj) return;

    for (int i = 0; string_keys[i] != NULL; i++) {
        value = json_get(obj, string_keys[i]);
        if (value) {
            json_set(obj, string_keys[i], value);
        }
    }

    value = json_get(obj, "layers");
    if (value) {
        json_set_raw(obj, "layers", value);
    }
}

static void registry_dedupe_index(JsonArray *arr) {
    if (!arr) return;

    for (int i = 0; i < arr->count; i++) {
        registry_normalize_image_object(&arr->objects[i]);
    }

    for (int i = arr->count - 1; i >= 0; i--) {
        const char *name = json_get(&arr->objects[i], "name");
        const char *tag = json_get(&arr->objects[i], "tag");

        if (!name || !tag) continue;

        for (int j = i - 1; j >= 0; j--) {
            const char *other_name = json_get(&arr->objects[j], "name");
            const char *other_tag = json_get(&arr->objects[j], "tag");

            if (other_name && other_tag &&
                strcmp(other_name, name) == 0 &&
                strcmp(other_tag, tag) == 0) {
                json_array_remove(arr, j);
                i--;
            }
        }
    }
}

static int registry_save_index(const JsonArray *arr) {
    char *arr_str;

    if (!arr) return -1;

    arr_str = json_array_stringify_pretty(arr);
    if (!arr_str) {
        fprintf(stderr, "Error: failed to serialize %s\n", REGISTRY_INDEX);
        return -1;
    }

    if (write_file(REGISTRY_INDEX, arr_str) != 0) {
        free(arr_str);
        return -1;
    }

    free(arr_str);
    return 0;
}

static int registry_load_index(JsonArray *arr) {
    char *data;
    char *normalized;

    if (!arr) return -1;

    data = read_file(REGISTRY_INDEX);
    if (!data) {
        fprintf(stderr, "Error: cannot read %s\n", REGISTRY_INDEX);
        return -1;
    }

    if (json_parse_array(data, arr) != 0) {
        fprintf(stderr, "Error: failed to parse %s\n", REGISTRY_INDEX);
        free(data);
        return -1;
    }
    free(data);

    registry_dedupe_index(arr);

    normalized = json_array_stringify_pretty(arr);
    if (!normalized) {
        fprintf(stderr, "Error: failed to serialize %s\n", REGISTRY_INDEX);
        return -1;
    }

    data = read_file(REGISTRY_INDEX);
    if (!data) {
        free(normalized);
        fprintf(stderr, "Error: cannot read %s\n", REGISTRY_INDEX);
        return -1;
    }

    if (strcmp(data, normalized) != 0 && write_file(REGISTRY_INDEX, normalized) != 0) {
        free(data);
        free(normalized);
        return -1;
    }

    free(data);
    free(normalized);
    return 0;
}

static int registry_has_flag(int argc, char *argv[], const char *flag) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return 1;
        }
    }
    return 0;
}

static void registry_apply_updates(JsonObject *target, const JsonObject *updates) {
    if (!target || !updates) return;

    for (int i = 0; i < updates->count; i++) {
        if (updates->pairs[i].is_string) {
            json_set(target, updates->pairs[i].key, updates->pairs[i].value);
        } else {
            json_set_raw(target, updates->pairs[i].key, updates->pairs[i].value);
        }
    }
}

static int registry_read_config_path(const char *config_path, JsonObject *config) {
    char *data;

    if (!config_path || !config) return -1;

    data = read_file(config_path);
    if (!data) return -1;

    if (json_parse(data, config) != 0) {
        free(data);
        return -1;
    }
    free(data);
    return 0;
}

static int registry_write_config_path(const char *config_path, const JsonObject *config) {
    char *data;

    if (!config_path || !config) return -1;

    data = json_stringify_pretty(config);
    if (!data) return -1;

    if (write_file(config_path, data) != 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}

static int registry_update_metadata(const char *name, const char *tag, const JsonObject *updates) {
    Image img;
    JsonObject config;
    JsonArray *arr;
    int found = 0;

    if (!name || !tag || !updates) return -1;

    if (registry_find(name, tag, &img) != 0) return -1;

    if (registry_read_config_path(img.config_path, &config) == 0) {
        registry_apply_updates(&config, updates);
        if (registry_write_config_path(img.config_path, &config) != 0) {
            return -1;
        }
    }

    arr = json_array_new();
    if (!arr) return -1;

    if (registry_load_index(arr) != 0) {
        json_array_free(arr);
        return -1;
    }

    for (int i = 0; i < arr->count; i++) {
        const char *n = json_get(&arr->objects[i], "name");
        const char *t = json_get(&arr->objects[i], "tag");

        if (n && t && strcmp(n, name) == 0 && strcmp(t, tag) == 0) {
            registry_apply_updates(&arr->objects[i], updates);
            found = 1;
            break;
        }
    }

    if (!found || registry_save_index(arr) != 0) {
        json_array_free(arr);
        return -1;
    }

    json_array_free(arr);
    return 0;
}

static int registry_compute_digest(const char *path, char *out, size_t out_len) {
    char cmd[MAX_CMD_LEN];
    char buffer[512];
    FILE *pipe;
    size_t idx = 0;

    if (!path || !out || out_len < 65) return -1;

#ifdef _WIN32
    if (format_buffer(cmd, sizeof(cmd), "certutil -hashfile \"%s\" SHA256", path) != 0)
        return -1;
#else
    if (format_buffer(cmd, sizeof(cmd), "sha256sum '%s'", path) != 0)
        return -1;
#endif

    pipe = popen(cmd, "r");
    if (!pipe) return -1;

    out[0] = '\0';
    while (fgets(buffer, sizeof(buffer), pipe)) {
        for (size_t i = 0; buffer[i] != '\0'; i++) {
            char ch = buffer[i];
            int is_hex = ((ch >= '0' && ch <= '9') ||
                          (ch >= 'a' && ch <= 'f') ||
                          (ch >= 'A' && ch <= 'F'));
            if (is_hex) {
                if (idx < out_len - 1) {
                    out[idx++] = (char) tolower((unsigned char) ch);
                }
                if (idx == 64) {
                    out[idx] = '\0';
                    pclose(pipe);
                    return 0;
                }
            }
        }
    }

    pclose(pipe);
    out[0] = '\0';
    return -1;
}

static int registry_compute_signature(const char *digest, const char *key,
                                      char *out, size_t out_len) {
    char tmp_path[] = "/tmp/mycontainer_sig_XXXXXX";
    char content[256];
    FILE *tmp_file;
    int fd;
    int result;

    if (!digest || !key || !out || out_len < 65) return -1;

    if (format_buffer(content, sizeof(content), "%s\n%s\n", digest, key) != 0) {
        return -1;
    }

    fd = mkstemp(tmp_path);
    if (fd < 0) {
        return -1;
    }

    tmp_file = fdopen(fd, "w");
    if (!tmp_file) {
        close(fd);
        remove(tmp_path);
        return -1;
    }

    fputs(content, tmp_file);
    fclose(tmp_file);

    result = registry_compute_digest(tmp_path, out, out_len);
    remove(tmp_path);
    return result;
}

static int registry_is_node_context(const char *context_dir) {
    char package_json[MAX_PATH_LEN];

    if (!context_dir) return 0;

    if (format_buffer(package_json, sizeof(package_json), "%s/package.json", context_dir) != 0) {
        return 0;
    }

    return file_exists(package_json);
}

static void registry_guess_node_cmd(const char *context_dir, char *out, size_t out_len) {
    char package_json[MAX_PATH_LEN];
    char candidate[MAX_PATH_LEN];
    char *data = NULL;
    const char *fallbacks[] = { "server.js", "index.js", "app.js", NULL };

    if (!out || out_len == 0) return;

    out[0] = '\0';
    if (!context_dir) {
        format_buffer(out, out_len, "node index.js");
        return;
    }

    if (format_buffer(package_json, sizeof(package_json), "%s/package.json", context_dir) == 0 &&
        file_exists(package_json)) {
        data = read_file(package_json);
        if (data) {
            if (strstr(data, "\"start\"")) {
                format_buffer(out, out_len, "npm start");
                free(data);
                return;
            }
            free(data);
        }
    }

    for (int i = 0; fallbacks[i] != NULL; i++) {
        if (format_buffer(candidate, sizeof(candidate), "%s/%s", context_dir, fallbacks[i]) == 0 &&
            file_exists(candidate)) {
            format_buffer(out, out_len, "node %s", fallbacks[i]);
            return;
        }
    }

    format_buffer(out, out_len, "node index.js");
}

/* ================================================================
 * REGISTRY LIST
 * ================================================================ */

int registry_list(Image *images, int *count) {
    if (!images || !count) return -1;
    *count = 0;

    if (registry_init() != 0) return -1;

    JsonArray *arr = json_array_new();
    if (!arr) return -1;

    if (registry_load_index(arr) != 0) {
        json_array_free(arr);
        return -1;
    }

    for (int i = 0; i < arr->count && i < MAX_IMAGES; i++) {
        json_object_to_image(&arr->objects[i], &images[i]);
    }
    *count = arr->count < MAX_IMAGES ? arr->count : MAX_IMAGES;

    json_array_free(arr);
    return 0;
}

/* ================================================================
 * REGISTRY PUSH
 * ================================================================ */

int registry_push(const char *name, const char *tag,
                  const char *rootfs_tar_path) {
    if (!name || !tag || !rootfs_tar_path) return -1;

    if (registry_init() != 0) return -1;

    /* Check source file exists */
    if (!file_exists(rootfs_tar_path)) {
        fprintf(stderr, "Error: rootfs tarball not found: %s\n", rootfs_tar_path);
        return -1;
    }

    /* Generate unique image ID */
    char id[ID_LEN + 1];
    generate_id(id);

    /* Create image directory */
    char image_dir[MAX_PATH_LEN];
    if (format_buffer(image_dir, sizeof(image_dir), "%s/%s_%s",
                      REGISTRY_IMAGES_DIR, name, tag) != 0) {
        fprintf(stderr, "Error: image path too long for %s:%s\n", name, tag);
        return -1;
    }

    if (mkdir_p(image_dir) != 0) {
        fprintf(stderr, "Error: failed to create image dir %s\n", image_dir);
        return -1;
    }

    /* Copy rootfs tarball */
    char rootfs_dest[MAX_PATH_LEN];
    if (format_buffer(rootfs_dest, sizeof(rootfs_dest), "%s/rootfs.tar.gz", image_dir) != 0) {
        fprintf(stderr, "Error: image rootfs path too long for %s:%s\n", name, tag);
        return -1;
    }

    if (copy_file(rootfs_tar_path, rootfs_dest) != 0) {
        fprintf(stderr, "Error: failed to copy rootfs tarball\n");
        return -1;
    }

    /* Get file size */
    long size = get_file_size(rootfs_dest);
    char size_str[16];
    format_size(size, size_str, sizeof(size_str));

    /* Get timestamp */
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    char digest[65] = "";
    if (registry_compute_digest(rootfs_dest, digest, sizeof(digest)) != 0) {
        fprintf(stderr, "Error: failed to compute image digest\n");
        return -1;
    }

    /* Write config.json */
    char config_path[MAX_PATH_LEN];
    if (format_buffer(config_path, sizeof(config_path), "%s/config.json", image_dir) != 0) {
        fprintf(stderr, "Error: image config path too long for %s:%s\n", name, tag);
        return -1;
    }

    JsonObject config;
    config.count = 0;
    json_set(&config, "name", name);
    json_set(&config, "tag", tag);
    json_set(&config, "runtime", "linux");
    json_set(&config, "cmd", "/bin/sh");
    json_set_raw(&config, "env", "[\"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\"]");
    json_set(&config, "workdir", "/");
    json_set_raw(&config, "layers", "1");
    json_set(&config, "digest", digest);
    json_set(&config, "created_at", timestamp);

    char *config_str = json_stringify_pretty(&config);
    if (config_str) {
        write_file(config_path, config_str);
        free(config_str);
    }

    /* Build Image struct */
    Image img;
    memset(&img, 0, sizeof(img));
    strncpy(img.id, id, sizeof(img.id) - 1);
    strncpy(img.name, name, sizeof(img.name) - 1);
    strncpy(img.tag, tag, sizeof(img.tag) - 1);
    strncpy(img.size, size_str, sizeof(img.size) - 1);
    img.layers = 1;
    strncpy(img.created_at, timestamp, sizeof(img.created_at) - 1);
    strncpy(img.runtime, "linux", sizeof(img.runtime) - 1);
    strncpy(img.cmd, "/bin/sh", sizeof(img.cmd) - 1);
    strncpy(img.digest, digest, sizeof(img.digest) - 1);
    strncpy(img.rootfs_path, rootfs_dest, sizeof(img.rootfs_path) - 1);
    strncpy(img.config_path, config_path, sizeof(img.config_path) - 1);

    /* Update images.json: remove existing entry for same name:tag, then append */
    JsonArray *arr = json_array_new();
    if (!arr) return -1;

    if (registry_load_index(arr) != 0) {
        json_array_free(arr);
        return -1;
    }

    /* Remove existing image with same name:tag to prevent duplicates */
    for (int i = 0; i < arr->count; ) {
        const char *n = json_get(&arr->objects[i], "name");
        const char *t = json_get(&arr->objects[i], "tag");
        if (n && t && strcmp(n, name) == 0 && strcmp(t, tag) == 0) {
            json_array_remove(arr, i);
            continue;
        }
        i++;
    }

    JsonObject img_obj;
    image_to_json_object(&img, &img_obj);
    json_array_append(arr, &img_obj);

    registry_dedupe_index(arr);
    if (registry_save_index(arr) != 0) {
        json_array_free(arr);
        return -1;
    }
    json_array_free(arr);

    printf("Pushed %s:%s successfully\n", name, tag);
    printf("Image ID: %s\n", id);
    return 0;
}

/* ================================================================
 * REGISTRY PULL
 * ================================================================ */

int registry_pull(const char *name, const char *tag,
                  const char *dest_path) {
    if (!name || !tag || !dest_path) return -1;

    Image img;
    if (registry_find(name, tag, &img) != 0) {
        fprintf(stderr, "Error: image %s:%s not found\n", name, tag);
        return -1;
    }

    printf("Pulling %s:%s...\n", name, tag);

    /* Create destination directory */
    if (mkdir_p(dest_path) != 0) {
        fprintf(stderr, "Error: cannot create %s\n", dest_path);
        return -1;
    }

    /* Extract rootfs tarball */
    char cmd[MAX_CMD_LEN];
    if (format_buffer(cmd, sizeof(cmd), "tar -xzf %s -C %s 2>/dev/null",
                      img.rootfs_path, dest_path) != 0) {
        fprintf(stderr, "Error: pull command too long for %s:%s\n", name, tag);
        return -1;
    }

    if (system(cmd) != 0) {
        fprintf(stderr, "Error: failed to extract rootfs\n");
        return -1;
    }

    printf("Extracted to %s\n", dest_path);
    return 0;
}

/* ================================================================
 * REGISTRY DELETE
 * ================================================================ */

int registry_delete(const char *name, const char *tag) {
    if (!name || !tag) return -1;

    if (registry_init() != 0) return -1;

    JsonArray *arr = json_array_new();
    if (!arr) return -1;

    if (registry_load_index(arr) != 0) {
        json_array_free(arr);
        return -1;
    }

    /* Find the image */
    int found = -1;
    for (int i = 0; i < arr->count; i++) {
        const char *n = json_get(&arr->objects[i], "name");
        const char *t = json_get(&arr->objects[i], "tag");
        if (n && t && strcmp(n, name) == 0 && strcmp(t, tag) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        fprintf(stderr, "Error: image %s:%s not found\n", name, tag);
        json_array_free(arr);
        return -1;
    }

    /* Remove the image directory */
    char image_dir[MAX_PATH_LEN];
    if (format_buffer(image_dir, sizeof(image_dir), "%s/%s_%s",
                      REGISTRY_IMAGES_DIR, name, tag) != 0) {
        fprintf(stderr, "Error: image path too long for %s:%s\n", name, tag);
        json_array_free(arr);
        return -1;
    }
    rmdir_recursive(image_dir);

    /* Remove from array */
    json_array_remove(arr, found);

    /* Write back */
    if (registry_save_index(arr) != 0) {
        json_array_free(arr);
        return -1;
    }
    json_array_free(arr);

    printf("Deleted %s:%s\n", name, tag);
    return 0;
}

/* ================================================================
 * REGISTRY BUILD
 * ================================================================ */

int registry_build(const char *context_dir,
                   const char *name,
                   const char *tag,
                   const char *runtime,
                   const char *cmd,
                   int json_output) {
    char tmp_dir[] = "/tmp/mycontainer_build_XXXXXX";
    char rootfs_tar[MAX_PATH_LEN];
    char build_cmd[MAX_CMD_LEN];
    char effective_runtime[32];
    char effective_cmd[256];
    JsonObject updates;
    Image img;
    int result = 0;

    if (!context_dir || !name || !tag) return -1;
    if (!dir_exists(context_dir)) {
        fprintf(stderr, "Error: build context not found: %s\n", context_dir);
        return -1;
    }

    if (runtime && strlen(runtime) > 0) {
        strncpy(effective_runtime, runtime, sizeof(effective_runtime) - 1);
        effective_runtime[sizeof(effective_runtime) - 1] = '\0';
    } else if (registry_is_node_context(context_dir)) {
        strncpy(effective_runtime, "nodejs", sizeof(effective_runtime) - 1);
        effective_runtime[sizeof(effective_runtime) - 1] = '\0';
    } else {
        strncpy(effective_runtime, "linux", sizeof(effective_runtime) - 1);
        effective_runtime[sizeof(effective_runtime) - 1] = '\0';
    }

    if (cmd && strlen(cmd) > 0) {
        strncpy(effective_cmd, cmd, sizeof(effective_cmd) - 1);
        effective_cmd[sizeof(effective_cmd) - 1] = '\0';
    } else if (strcmp(effective_runtime, "nodejs") == 0) {
        registry_guess_node_cmd(context_dir, effective_cmd, sizeof(effective_cmd));
    } else {
        strncpy(effective_cmd, "/bin/sh", sizeof(effective_cmd) - 1);
        effective_cmd[sizeof(effective_cmd) - 1] = '\0';
    }

    if (mkdtemp(tmp_dir) == NULL) {
        fprintf(stderr, "Error: cannot create build directory: %s\n", strerror(errno));
        return -1;
    }

    if (format_buffer(rootfs_tar, sizeof(rootfs_tar), "%s/rootfs.tar.gz", tmp_dir) != 0 ||
        format_buffer(build_cmd, sizeof(build_cmd), "tar -czf %s -C %s . 2>/dev/null",
                      rootfs_tar, context_dir) != 0) {
        rmdir_recursive(tmp_dir);
        return -1;
    }

    if (system(build_cmd) != 0) {
        fprintf(stderr, "Error: failed to package build context\n");
        rmdir_recursive(tmp_dir);
        return -1;
    }

    if (json_output) {
        int saved = dup(1);
        int devnull = open("/dev/null", O_WRONLY);
        if (saved < 0 || devnull < 0) {
            if (saved >= 0) close(saved);
            if (devnull >= 0) close(devnull);
            rmdir_recursive(tmp_dir);
            return -1;
        }
        dup2(devnull, 1);
        close(devnull);
        result = registry_push(name, tag, rootfs_tar);
        fflush(stdout);
        dup2(saved, 1);
        close(saved);
    } else {
        result = registry_push(name, tag, rootfs_tar);
    }
    rmdir_recursive(tmp_dir);
    if (result != 0) return -1;

    updates.count = 0;
    json_set(&updates, "runtime", effective_runtime);
    json_set(&updates, "cmd", effective_cmd);
    if (strcmp(effective_runtime, "nodejs") == 0) {
        json_set_raw(&updates, "env",
                     "[\"NODE_ENV=production\",\"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\"]");
    }

    if (registry_update_metadata(name, tag, &updates) != 0) {
        return -1;
    }

    if (registry_find(name, tag, &img) != 0) {
        return -1;
    }

    if (json_output) {
        JsonObject result_obj;
        image_to_json_object(&img, &result_obj);
        char *json = json_stringify_pretty(&result_obj);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    } else {
        printf("Built %s:%s from %s\n", name, tag, context_dir);
        printf("Runtime: %s\n", effective_runtime);
        printf("Default CMD: %s\n", effective_cmd);
        printf("Image ID: %s\n", img.id);
    }

    return 0;
}

/* ================================================================
 * REGISTRY SIGN / VERIFY
 * ================================================================ */

int registry_sign(const char *name, const char *tag,
                  const char *key, int json_output) {
    Image img;
    char digest[65];
    char signature[65];
    char signed_at[32];
    JsonObject updates;

    if (!name || !tag || !key || strlen(key) == 0) {
        fprintf(stderr, "Error: signing requires a shared secret (--key or MYCONTAINER_SIGN_KEY)\n");
        return -1;
    }

    if (registry_find(name, tag, &img) != 0) return -1;
    if (registry_compute_digest(img.rootfs_path, digest, sizeof(digest)) != 0 ||
        registry_compute_signature(digest, key, signature, sizeof(signature)) != 0) {
        fprintf(stderr, "Error: failed to compute image signature\n");
        return -1;
    }

    get_timestamp(signed_at, sizeof(signed_at));

    updates.count = 0;
    json_set(&updates, "digest", digest);
    json_set(&updates, "signature", signature);
    json_set(&updates, "signed_at", signed_at);

    if (registry_update_metadata(name, tag, &updates) != 0) {
        return -1;
    }

    if (json_output) {
        JsonObject result;
        result.count = 0;
        json_set(&result, "name", name);
        json_set(&result, "tag", tag);
        json_set(&result, "digest", digest);
        json_set(&result, "signature", signature);
        json_set(&result, "signed_at", signed_at);
        char *json = json_stringify_pretty(&result);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    } else {
        printf("Signed %s:%s\n", name, tag);
        printf("Digest:    %s\n", digest);
        printf("Signature: %s\n", signature);
    }

    return 0;
}

int registry_verify(const char *name, const char *tag,
                    const char *key, int json_output) {
    Image img;
    char digest[65];
    char expected_signature[65] = "";
    int digest_match;
    int signature_present;
    int signature_valid = 0;
    int trusted = 0;

    if (!name || !tag) return -1;
    if (registry_find(name, tag, &img) != 0) return -1;

    if (registry_compute_digest(img.rootfs_path, digest, sizeof(digest)) != 0) {
        fprintf(stderr, "Error: failed to compute image digest\n");
        return -1;
    }

    digest_match = (strlen(img.digest) > 0 && strcmp(img.digest, digest) == 0);
    signature_present = (strlen(img.signature) > 0);

    if (signature_present && key && strlen(key) > 0 &&
        registry_compute_signature(digest, key, expected_signature, sizeof(expected_signature)) == 0) {
        signature_valid = (strcmp(expected_signature, img.signature) == 0);
    }

    trusted = digest_match && (!signature_present || (key && strlen(key) > 0 && signature_valid));

    if (json_output) {
        JsonObject result;
        result.count = 0;
        json_set(&result, "name", name);
        json_set(&result, "tag", tag);
        json_set(&result, "digest", digest);
        json_set_raw(&result, "digest_match", digest_match ? "true" : "false");
        json_set_raw(&result, "signature_present", signature_present ? "true" : "false");
        json_set_raw(&result, "signature_valid", signature_valid ? "true" : "false");
        json_set_raw(&result, "trusted", trusted ? "true" : "false");
        char *json = json_stringify_pretty(&result);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    } else {
        printf("Verify %s:%s\n", name, tag);
        printf("Digest match:      %s\n", digest_match ? "yes" : "no");
        printf("Signature present: %s\n", signature_present ? "yes" : "no");
        if (signature_present) {
            if (key && strlen(key) > 0) {
                printf("Signature valid:   %s\n", signature_valid ? "yes" : "no");
            } else {
                printf("Signature valid:   key required\n");
            }
        }
        printf("Trusted:           %s\n", trusted ? "yes" : "no");
    }

    return trusted ? 0 : 1;
}

/* ================================================================
 * REGISTRY INSPECT
 * ================================================================ */

int registry_inspect(const char *name, const char *tag, int json_output) {
    if (!name || !tag) return -1;

    Image img;
    if (registry_find(name, tag, &img) != 0) {
        fprintf(stderr, "Error: image %s:%s not found\n", name, tag);
        return -1;
    }

    if (json_output) {
        /* Read config.json and print it */
        JsonObject obj;
        image_to_json_object(&img, &obj);

        /* Also include config.json contents */
        char *config_data = read_file(img.config_path);
        if (config_data) {
            JsonObject config;
            json_parse(config_data, &config);
            free(config_data);

            const char *runtime = json_get(&config, "runtime");
            if (runtime) json_set(&obj, "runtime", runtime);
            const char *cmd = json_get(&config, "cmd");
            if (cmd) json_set(&obj, "cmd", cmd);
            const char *env = json_get(&config, "env");
            if (env) json_set_raw(&obj, "env", env);
            const char *workdir = json_get(&config, "workdir");
            if (workdir) json_set(&obj, "workdir", workdir);
            const char *digest = json_get(&config, "digest");
            if (digest) json_set(&obj, "digest", digest);
            const char *signature = json_get(&config, "signature");
            if (signature) json_set(&obj, "signature", signature);
            const char *signed_at = json_get(&config, "signed_at");
            if (signed_at) json_set(&obj, "signed_at", signed_at);
        }

        char *str = json_stringify_pretty(&obj);
        if (str) {
            printf("%s\n", str);
            free(str);
        }
    } else {
        char ago[64];
        time_ago(img.created_at, ago, sizeof(ago));

        printf("Image:       %s:%s\n", img.name, img.tag);
        printf("ID:          %s\n", img.id);
        printf("Size:        %s\n", img.size);
        printf("Layers:      %d\n", img.layers);
        printf("Created:     %s (%s)\n", img.created_at, ago);
        printf("Rootfs:      %s\n", img.rootfs_path);
        printf("Config:      %s\n", img.config_path);
        if (strlen(img.digest) > 0) {
            printf("Digest:      %s\n", img.digest);
        }
        if (strlen(img.signature) > 0) {
            printf("Signature:   %s\n", img.signature);
        }

        /* Print config details */
        char *config_data = read_file(img.config_path);
        if (config_data) {
            JsonObject config;
            json_parse(config_data, &config);
            free(config_data);

            const char *runtime = json_get(&config, "runtime");
            if (runtime) printf("Runtime:     %s\n", runtime);
            const char *cmd = json_get(&config, "cmd");
            if (cmd) printf("Default CMD: %s\n", cmd);
            const char *env = json_get(&config, "env");
            if (env) printf("Env:         %s\n", env);
            const char *workdir = json_get(&config, "workdir");
            if (workdir) printf("Workdir:     %s\n", workdir);
            const char *signed_at = json_get(&config, "signed_at");
            if (signed_at) printf("Signed At:   %s\n", signed_at);
        }
    }

    return 0;
}

/* ================================================================
 * REGISTRY FIND
 * ================================================================ */

int registry_find(const char *name, const char *tag, Image *out) {
    if (!name || !tag || !out) return -1;

    Image images[MAX_IMAGES];
    int count = 0;

    if (registry_list(images, &count) != 0) return -1;

    for (int i = 0; i < count; i++) {
        if (strcmp(images[i].name, name) == 0 &&
            strcmp(images[i].tag, tag) == 0) {
            memcpy(out, &images[i], sizeof(Image));
            return 0;
        }
    }

    return -1;
}

/* ================================================================
 * CLI WRAPPERS
 * ================================================================ */

int registry_list_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    Image images[MAX_IMAGES];
    int count = 0;

    if (registry_list(images, &count) != 0) return 1;

    if (json_output) {
        JsonArray *arr = json_array_new();
        if (!arr) return 1;
        for (int i = 0; i < count; i++) {
            JsonObject obj;
            image_to_json_object(&images[i], &obj);
            json_array_append(arr, &obj);
        }
        char *str = json_array_stringify_pretty(arr);
        if (str) {
            printf("%s\n", str);
            free(str);
        }
        json_array_free(arr);
    } else {
        printf("%-12s %-12s %-10s %-8s %-8s %s\n",
               "IMAGE ID", "NAME", "TAG", "SIZE", "LAYERS", "CREATED");
        printf("%-12s %-12s %-10s %-8s %-8s %s\n",
               "--------", "----", "---", "----", "------", "-------");

        for (int i = 0; i < count; i++) {
            char ago[64];
            time_ago(images[i].created_at, ago, sizeof(ago));
            printf("%-12s %-12s %-10s %-8s %-8d %s\n",
                   images[i].id,
                   images[i].name,
                   images[i].tag,
                   images[i].size,
                   images[i].layers,
                   ago);
        }

        if (count == 0) {
            printf("\nNo images found in registry.\n");
        }
    }

    return 0;
}

int registry_push_cmd(const char *name_tag, const char *rootfs_path) {
    if (!name_tag || !rootfs_path) {
        fprintf(stderr, "Usage: mycontainer image push <name>:<tag> <rootfs.tar.gz>\n");
        return 1;
    }

    char name[64], tag[32];
    if (split_name_tag(name_tag, name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    return registry_push(name, tag, rootfs_path);
}

int registry_build_cmd(int argc, char *argv[]) {
    const char *cmd = get_flag_value(argc, argv, "--cmd");
    int json_output = has_json_flag(argc, argv);
    int node_hint = registry_has_flag(argc, argv, "--node");
    const char *runtime = node_hint ? "nodejs" : NULL;

    if (argc < 5) {
        fprintf(stderr, "Usage: mycontainer image build <context_dir> <name>:<tag> [--node] [--cmd=\"...\"] [--json]\n");
        return 1;
    }

    char name[64], tag[32];
    if (split_name_tag(argv[4], name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    return registry_build(argv[3], name, tag, runtime, cmd, json_output);
}

int registry_pull_cmd(const char *name_tag) {
    if (!name_tag) {
        fprintf(stderr, "Usage: mycontainer image pull <name>:<tag>\n");
        return 1;
    }

    char name[64], tag[32];
    if (split_name_tag(name_tag, name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    /* Default pull location */
    char dest[MAX_PATH_LEN];
    if (format_buffer(dest, sizeof(dest), "./pulled_%s_%s", name, tag) != 0) {
        fprintf(stderr, "Error: destination path too long for %s:%s\n", name, tag);
        return 1;
    }

    return registry_pull(name, tag, dest);
}

int registry_delete_cmd(const char *name_tag) {
    if (!name_tag) {
        fprintf(stderr, "Usage: mycontainer image rm <name>:<tag>\n");
        return 1;
    }

    char name[64], tag[32];
    if (split_name_tag(name_tag, name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    return registry_delete(name, tag);
}

int registry_inspect_cmd(const char *name_tag, int json_output) {
    if (!name_tag) {
        fprintf(stderr, "Usage: mycontainer image inspect <name>:<tag>\n");
        return 1;
    }

    char name[64], tag[32];
    if (split_name_tag(name_tag, name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    return registry_inspect(name, tag, json_output);
}

int registry_sign_cmd(int argc, char *argv[]) {
    const char *key = get_flag_value(argc, argv, "--key");
    int json_output = has_json_flag(argc, argv);

    if (argc < 4) {
        fprintf(stderr, "Usage: mycontainer image sign <name>:<tag> [--key=<secret>] [--json]\n");
        return 1;
    }

    if (!key || strlen(key) == 0) {
        key = getenv("MYCONTAINER_SIGN_KEY");
    }

    char name[64], tag[32];
    if (split_name_tag(argv[3], name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    return registry_sign(name, tag, key, json_output);
}

int registry_verify_cmd(int argc, char *argv[]) {
    const char *key = get_flag_value(argc, argv, "--key");
    int json_output = has_json_flag(argc, argv);

    if (argc < 4) {
        fprintf(stderr, "Usage: mycontainer image verify <name>:<tag> [--key=<secret>] [--json]\n");
        return 1;
    }

    if (!key || strlen(key) == 0) {
        key = getenv("MYCONTAINER_SIGN_KEY");
    }

    char name[64], tag[32];
    if (split_name_tag(argv[3], name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    return registry_verify(name, tag, key, json_output);
}
