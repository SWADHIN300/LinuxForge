/*
 * export.c — Container Filesystem Export & Image Import
 *
 * PURPOSE:
 *   Provide import/export operations that bridge the container runtime
 *   to external tooling and backup workflows.
 *
 * EXPORT (container → tar.gz):
 *   Captures a live container's rootfs into a portable tar.gz archive.
 *   Uses: tar -czf <output> -C <container.rootfs> .
 *   The resulting archive is a minimal OCI-compatible rootfs bundle that
 *   can be used to recreate the container's filesystem state on any host.
 *
 * IMPORT (tar.gz → registry image):
 *   Validates a tar archive and registers it in the local image registry
 *   under a user-supplied name:tag, making it immediately available for
 *   use with `mycontainer run --image=<name:tag>`.
 *
 *   Validation: tar -tzf <archive> — ensures the archive is well-formed
 *   before attempting the registry push.
 *
 * WORKFLOW DIAGRAM:
 *   Export:  containers/<id>/rootfs/ → tar.gz → output file
 *   Import:  tar.gz → validate → registry_push() → registry/images/<name:tag>/
 *
 * FUNCTIONS:
 *   container_export() — tar the running container's rootfs to a file
 *   container_import() — validate + register a tar archive as a new image
 *   export_cmd()       — CLI dispatch for `mycontainer export`
 *   import_cmd()       — CLI dispatch for `mycontainer import`
 */

#define _GNU_SOURCE
#include "../include/export.h"

int container_export(const char *container_id, const char *output_path, int json_output) {
    ContainerState state;
    char cmd[MAX_CMD_LEN];
    char size[16];
    long bytes;

    if (!container_id || !output_path) return -1;
    if (read_container_state(container_id, &state) != 0) return -1;
    if (!dir_exists(state.rootfs)) {
        fprintf(stderr, "Error: rootfs not found for %s\n", container_id);
        return -1;
    }

    if (format_buffer(cmd, sizeof(cmd),
                      "tar -czf \"%s\" -C \"%s\" .",
                      output_path, state.rootfs) != 0) {
        fprintf(stderr, "Error: export command too long for %s\n", container_id);
        return -1;
    }

    if (system(cmd) != 0) {
        fprintf(stderr, "Error: failed to export %s\n", container_id);
        return -1;
    }

    bytes = get_file_size(output_path);
    format_size(bytes, size, sizeof(size));

    if (json_output) {
        printf("{\n");
        printf("  \"success\": true,\n");
        printf("  \"container_id\": \"%s\",\n", container_id);
        printf("  \"output_path\": \"%s\",\n", output_path);
        printf("  \"size\": \"%s\"\n", size);
        printf("}\n");
    } else {
        printf("Exported %s to %s\n", container_id, output_path);
        printf("Size: %s\n", size);
    }

    return 0;
}

int container_import(const char *tar_path, const char *name, const char *tag, int json_output) {
    char list_cmd[MAX_CMD_LEN];

    if (!tar_path || !name || !tag) return -1;
    if (!file_exists(tar_path)) {
        fprintf(stderr, "Error: import tarball not found: %s\n", tar_path);
        return -1;
    }

    if (format_buffer(list_cmd, sizeof(list_cmd), "tar -tzf \"%s\" > /dev/null 2>&1", tar_path) != 0) {
        return -1;
    }

    if (system(list_cmd) != 0) {
        fprintf(stderr, "Error: invalid tar archive: %s\n", tar_path);
        return -1;
    }

    if (registry_push(name, tag, tar_path) != 0) {
        return -1;
    }

    if (json_output) {
        printf("{\n");
        printf("  \"success\": true,\n");
        printf("  \"name\": \"%s\",\n", name);
        printf("  \"tag\": \"%s\",\n", tag);
        printf("  \"source\": \"%s\"\n", tar_path);
        printf("}\n");
    } else {
        printf("Imported %s as %s:%s\n", tar_path, name, tag);
    }

    return 0;
}

int export_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    if (argc < 4) {
        fprintf(stderr, "Usage: mycontainer export <container_id> <output.tar.gz> [--json]\n");
        return 1;
    }

    return container_export(argv[2], argv[3], json_output);
}

int import_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    if (argc < 4) {
        fprintf(stderr, "Usage: mycontainer import <tar.gz> <name:tag> [--json]\n");
        return 1;
    }

    char name[64];
    char tag[32];
    if (split_name_tag(argv[3], name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    return container_import(argv[2], name, tag, json_output);
}
