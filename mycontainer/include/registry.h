/*
 * registry.h — Container image registry
 *
 * Manages a local image store with push, pull, list,
 * delete, and inspect operations.
 */

#ifndef REGISTRY_H
#define REGISTRY_H

#include "utils.h"

/* ---- Constants ---- */
#define REGISTRY_DIR        "registry"
#define REGISTRY_IMAGES_DIR "registry/images"
#define REGISTRY_INDEX      "registry/images.json"
#define MAX_IMAGES          128

/* ---- Image Metadata ---- */
typedef struct {
    char id[16];            /* "img_a3f9c2" */
    char name[64];          /* "alpine" */
    char tag[32];           /* "3.18" */
    char size[16];          /* "5MB" */
    int  layers;            /* number of layers */
    char created_at[32];    /* ISO timestamp */
    char runtime[32];       /* "linux", "nodejs" */
    char cmd[256];          /* default command */
    char digest[65];        /* sha256 hex */
    char signature[65];     /* derived signature */
    char signed_at[32];     /* ISO timestamp */
    char rootfs_path[MAX_PATH_LEN];  /* path to rootfs.tar.gz */
    char config_path[MAX_PATH_LEN];  /* path to config.json */
} Image;

/* ---- Core Registry Functions ---- */

/*
 * Initialize the registry directory structure.
 * Creates registry/, registry/images/, and an empty images.json.
 * Returns 0 on success, -1 on error.
 */
int registry_init(void);

/*
 * List all images in the registry.
 * Fills the images array and sets *count.
 * Returns 0 on success, -1 on error.
 */
int registry_list(Image *images, int *count);

/*
 * Push a new image to the registry.
 * - name: image name (e.g. "alpine")
 * - tag: image tag (e.g. "3.18")
 * - rootfs_tar_path: path to rootfs.tar.gz to import
 * Returns 0 on success, -1 on error.
 */
int registry_push(const char *name, const char *tag,
                  const char *rootfs_tar_path);

/*
 * Build an image from a directory context.
 * Supports generic contexts and Node.js-aware defaults.
 */
int registry_build(const char *context_dir,
                   const char *name,
                   const char *tag,
                   const char *runtime,
                   const char *cmd,
                   int json_output);

/*
 * Pull an image from the registry, extracting to dest_path.
 * - name: image name
 * - tag: image tag
 * - dest_path: directory to extract rootfs into
 * Returns 0 on success, -1 if not found.
 */
int registry_pull(const char *name, const char *tag,
                  const char *dest_path);

/*
 * Delete an image from the registry.
 * Returns 0 on success, -1 if not found.
 */
int registry_delete(const char *name, const char *tag);

/*
 * Inspect an image — print full details.
 * If json_output is non-zero, print JSON; otherwise formatted text.
 * Returns 0 on success, -1 if not found.
 */
int registry_inspect(const char *name, const char *tag, int json_output);

/*
 * Sign or verify an image using a digest and shared secret.
 */
int registry_sign(const char *name, const char *tag,
                  const char *key, int json_output);
int registry_verify(const char *name, const char *tag,
                    const char *key, int json_output);

/*
 * Find an image by name and tag.
 * Returns the index in images.json, or -1 if not found.
 */
int registry_find(const char *name, const char *tag, Image *out);

/* ---- CLI Wrapper Functions ---- */

int registry_list_cmd(int argc, char *argv[]);
int registry_push_cmd(const char *name_tag, const char *rootfs_path);
int registry_build_cmd(int argc, char *argv[]);
int registry_pull_cmd(const char *name_tag);
int registry_delete_cmd(const char *name_tag);
int registry_inspect_cmd(const char *name_tag, int json_output);
int registry_sign_cmd(int argc, char *argv[]);
int registry_verify_cmd(int argc, char *argv[]);

#endif /* REGISTRY_H */
