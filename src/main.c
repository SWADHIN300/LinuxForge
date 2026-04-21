/*
 * main.c — mycontainer CLI Entry Point & Command Dispatcher
 *
 * PURPOSE:
 *   Parse the user's command-line arguments and route execution to the
 *   appropriate subsystem handler. This file is the single entry point
 *   for the mycontainer binary (compiled from src/ via the Makefile).
 *
 * USAGE SUMMARY:
 *   ./mycontainer <command> [options]
 *
 *   Core commands:
 *     run        → run_container()        (container.c)
 *     logs       → logs_cmd()             (logs.c)
 *     stats      → stats_cmd()            (stats.c)
 *     health     → health_cmd()           (health.c)
 *     volume     → volume_cmd()           (volume.c)
 *     env        → env_list_cmd()         (container.c)
 *     rename     → container_rename()     (container.c)
 *
 *   Image management:
 *     image ls/push/build/pull/rm/inspect → registry_*.c
 *     export / import                     → export.c
 *
 *   Networking & DNS:
 *     network init/ls/connect/disconnect/inspect/destroy → network.c
 *     dns ls/update                                      → dns.c
 *
 *   Orchestration:
 *     stack up/down/status/ls → stack.c
 *
 *   Security & Checkpoints:
 *     security ls/inspect → security.c
 *     checkpoint / restore → checkpoint.c
 *
 *   Commit:
 *     commit <id> <name:tag>   → commit.c
 *     commit ls / commit history
 *
 * JSON MODE:
 *   Append --json to any supported command to receive machine-readable
 *   JSON output instead of human-readable text. The Node.js bridge
 *   (simulatorBridge.js) always passes --json.
 *
 * DESIGN:
 *   No global state is used. Each command handler receives argc/argv
 *   and is responsible for its own argument validation and error output.
 *   All handlers return 0 on success, non-zero on failure.
 */

#define _GNU_SOURCE
#include "../include/checkpoint.h"
#include "../include/commit.h"
#include "../include/container.h"
#include "../include/dns.h"
#include "../include/export.h"
#include "../include/health.h"
#include "../include/logs.h"
#include "../include/network.h"
#include "../include/registry.h"
#include "../include/security.h"
#include "../include/stack.h"
#include "../include/stats.h"
#include "../include/volume.h"

#include <stdio.h>
#include <string.h>

static void print_usage(void) {
    printf(
        "mycontainer - container simulator\n\n"
        "Usage:\n"
        "  ./mycontainer <command> [options]\n\n"
        "Core:\n"
        "  run [--name=<n>] [--image=<name:tag>] [--rootless|--privileged]\n"
        "      [--restart=<no|always|on-failure>] [--security=<profile>]\n"
        "      [--env=KEY=VALUE] [--volume=host:container[:ro|rw]] <command>\n"
        "  rename <id> <new-name>\n"
        "  env <id>\n"
        "  logs <id> [--tail=N|--follow|--clear]\n"
        "  stats <id> [--watch|--history=N]\n"
        "  health <id> [--run]\n"
        "  volume ls <id>\n\n"
        "Images:\n"
        "  image ls|push|build|pull|rm|inspect|sign|verify\n"
        "  export <id> <backup.tar.gz>\n"
        "  import <backup.tar.gz> <name:tag>\n\n"
        "Networking:\n"
        "  network init|ls|connect|disconnect|inspect|destroy\n"
        "  dns ls|update\n\n"
        "Stacks:\n"
        "  stack up <file>\n"
        "  stack down <file|name>\n"
        "  stack status <name>\n"
        "  stack ls\n\n"
        "Security / Checkpoints:\n"
        "  security ls|inspect <profile>\n"
        "  checkpoint <id> <dir>\n"
        "  checkpoint ls\n"
        "  restore <dir> <new-name>\n\n"
        "Use --json on supported commands for backend-friendly output.\n");
}

static void print_version(void) {
    printf("mycontainer v2.0.0\n");
}

int main(int argc, char *argv[]) {
    int json_output;

    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        print_version();
        return 0;
    }

    json_output = has_json_flag(argc, argv);

    if (strcmp(argv[1], "run") == 0) return run_container(argc, argv);
    if (strcmp(argv[1], "logs") == 0) return logs_cmd(argc, argv);
    if (strcmp(argv[1], "stats") == 0) return stats_cmd(argc, argv);
    if (strcmp(argv[1], "health") == 0) return health_cmd(argc, argv);
    if (strcmp(argv[1], "volume") == 0) return volume_cmd(argc, argv);
    if (strcmp(argv[1], "env") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: mycontainer env <id> [--json]\n");
            return 1;
        }
        return env_list_cmd(argv[2], json_output);
    }
    if (strcmp(argv[1], "export") == 0) return export_cmd(argc, argv);
    if (strcmp(argv[1], "import") == 0) return import_cmd(argc, argv);
    if (strcmp(argv[1], "stack") == 0) return stack_cmd(argc, argv);
    if (strcmp(argv[1], "security") == 0) return security_cmd(argc, argv);
    if (strcmp(argv[1], "dns") == 0) return dns_cmd(argc, argv);
    if (strcmp(argv[1], "checkpoint") == 0) return checkpoint_cmd(argc, argv);
    if (strcmp(argv[1], "restore") == 0) return restore_cmd(argc, argv);
    if (strcmp(argv[1], "rename") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: mycontainer rename <id> <new-name> [--json]\n");
            return 1;
        }
        return container_rename(argv[2], argv[3], json_output);
    }

    if (strcmp(argv[1], "image") == 0 || strcmp(argv[1], "images") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: mycontainer image <ls|push|build|pull|rm|inspect|sign|verify>\n");
            return 1;
        }

        if (strcmp(argv[2], "ls") == 0 || strcmp(argv[2], "list") == 0) {
            return registry_list_cmd(argc, argv);
        }
        if (strcmp(argv[2], "push") == 0) {
            if (argc < 5) {
                fprintf(stderr, "Usage: mycontainer image push <name:tag> <rootfs.tar.gz>\n");
                return 1;
            }
            return registry_push_cmd(argv[3], argv[4]);
        }
        if (strcmp(argv[2], "build") == 0) return registry_build_cmd(argc, argv);
        if (strcmp(argv[2], "pull") == 0) {
            if (argc < 4) return 1;
            return registry_pull_cmd(argv[3]);
        }
        if (strcmp(argv[2], "rm") == 0 || strcmp(argv[2], "remove") == 0) {
            if (argc < 4) return 1;
            return registry_delete_cmd(argv[3]);
        }
        if (strcmp(argv[2], "inspect") == 0) {
            if (argc < 4) return 1;
            return registry_inspect_cmd(argv[3], json_output);
        }
        if (strcmp(argv[2], "sign") == 0) return registry_sign_cmd(argc, argv);
        if (strcmp(argv[2], "verify") == 0) return registry_verify_cmd(argc, argv);

        fprintf(stderr, "Unknown image command: %s\n", argv[2]);
        return 1;
    }

    if (strcmp(argv[1], "commit") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: mycontainer commit <id> <name:tag> [--description=\"...\"]\n");
            return 1;
        }
        if (strcmp(argv[2], "ls") == 0 || strcmp(argv[2], "list") == 0) {
            return commit_list_containers(json_output);
        }
        if (strcmp(argv[2], "history") == 0) {
            return commit_history(json_output);
        }
        if (argc < 4) {
            fprintf(stderr, "Usage: mycontainer commit <id> <name:tag> [--description=\"...\"]\n");
            return 1;
        }
        return commit_container_cmd(argv[2], argv[3],
                                    get_flag_value(argc, argv, "--description"),
                                    json_output);
    }

    if (strcmp(argv[1], "network") == 0 || strcmp(argv[1], "net") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: mycontainer network <init|ls|connect|disconnect|inspect|destroy>\n");
            return 1;
        }
        if (strcmp(argv[2], "init") == 0) return network_init();
        if (strcmp(argv[2], "ls") == 0 || strcmp(argv[2], "list") == 0) return network_list(json_output);
        if (strcmp(argv[2], "connect") == 0 && argc >= 5) return network_connect(argv[3], argv[4]);
        if (strcmp(argv[2], "disconnect") == 0 && argc >= 5) return network_disconnect(argv[3], argv[4]);
        if (strcmp(argv[2], "inspect") == 0 && argc >= 4) return network_inspect(argv[3], json_output);
        if (strcmp(argv[2], "destroy") == 0) return network_destroy();
        fprintf(stderr, "Unknown network command: %s\n", argv[2]);
        return 1;
    }

    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    fprintf(stderr, "Run './mycontainer --help' for usage.\n");
    return 1;
}
