/*
 * network.c — Container-to-container networking
 *
 * Manages a Linux bridge (mycontainer0), veth pairs,
 * IP assignment, and iptables rules for inter-container
 * communication via CLONE_NEWNET namespaces.
 *
 * Architecture:
 *   Host Bridge (mycontainer0) - 172.18.0.1/24
 *     ├── veth_<id1> ←→ eth0 (container 1: 172.18.0.2)
 *     └── veth_<id2> ←→ eth0 (container 2: 172.18.0.3)
 */

#define _GNU_SOURCE
#include "../include/network.h"

/* ================================================================
 * STATE I/O
 *
 * Serialize/deserialize NetworkState to/from JSON.
 * Format:
 * {
 *   "bridge": { "name": "...", "ip": "...", "subnet": "..." },
 *   "next_ip_octet": 2,
 *   "connections": [ ... ]
 * }
 * ================================================================ */

/* Helper: extract a substring between matching braces/brackets starting at *start.
 * Returns a malloc'd string. Caller must free(). */
static char *extract_balanced(const char *start, char open, char close) {
    if (!start || *start != open) return NULL;
    int depth = 0;
    const char *p = start;
    do {
        if (*p == open) depth++;
        if (*p == close) depth--;
        p++;
    } while (*p && depth > 0);
    size_t len = p - start;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* Helper: find a JSON key in raw text and return pointer to its value */
static const char *find_json_key(const char *data, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(data, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    /* skip whitespace and colon */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':')) p++;
    return p;
}

int network_read_state(NetworkState *state) {
    if (!state) return -1;

    memset(state, 0, sizeof(NetworkState));

    /* Defaults */
    strncpy(state->bridge_name, BRIDGE_NAME, sizeof(state->bridge_name) - 1);
    strncpy(state->bridge_ip, BRIDGE_IP, sizeof(state->bridge_ip) - 1);
    strncpy(state->bridge_ipv6, BRIDGE_IPV6, sizeof(state->bridge_ipv6) - 1);
    strncpy(state->subnet, BRIDGE_SUBNET, sizeof(state->subnet) - 1);
    strncpy(state->ipv6_subnet, BRIDGE_IPV6_SUBNET, sizeof(state->ipv6_subnet) - 1);
    state->next_ip_octet = 2;
    state->next_ipv6_suffix = 2;

    if (!file_exists(NETWORK_STATE)) {
        return 0; /* Return defaults */
    }

    char *data = read_file(NETWORK_STATE);
    if (!data) return -1;

    /* Parse bridge section */
    const char *bridge_start = find_json_key(data, "bridge");
    if (bridge_start && *bridge_start == '{') {
        char *bridge_str = extract_balanced(bridge_start, '{', '}');
        if (bridge_str) {
            JsonObject bridge;
            json_parse(bridge_str, &bridge);
            free(bridge_str);
            const char *v;
            if ((v = json_get(&bridge, "name")))
                strncpy(state->bridge_name, v, sizeof(state->bridge_name) - 1);
            if ((v = json_get(&bridge, "ip")))
                strncpy(state->bridge_ip, v, sizeof(state->bridge_ip) - 1);
            if ((v = json_get(&bridge, "ipv6")))
                strncpy(state->bridge_ipv6, v, sizeof(state->bridge_ipv6) - 1);
            if ((v = json_get(&bridge, "subnet")))
                strncpy(state->subnet, v, sizeof(state->subnet) - 1);
            if ((v = json_get(&bridge, "ipv6_subnet")))
                strncpy(state->ipv6_subnet, v, sizeof(state->ipv6_subnet) - 1);
        }
    }

    /* Parse next_ip_octet */
    const char *octet_start = find_json_key(data, "next_ip_octet");
    if (octet_start) {
        state->next_ip_octet = atoi(octet_start);
    }

    const char *ipv6_suffix_start = find_json_key(data, "next_ipv6_suffix");
    if (ipv6_suffix_start) {
        state->next_ipv6_suffix = atoi(ipv6_suffix_start);
    }

    /* Parse connections array */
    const char *conn_start = find_json_key(data, "connections");
    if (conn_start && *conn_start == '[') {
        char *conn_str = extract_balanced(conn_start, '[', ']');
        if (conn_str) {
            JsonArray *conns = json_array_new();
            if (conns) {
                json_parse_array(conn_str, conns);

                for (int i = 0; i < conns->count && i < MAX_CONNECTIONS; i++) {
                    NetConnection *c = &state->connections[i];
                    const char *v;

                    if ((v = json_get(&conns->objects[i], "container_id")))
                        strncpy(c->container_id, v, sizeof(c->container_id) - 1);
                    if ((v = json_get(&conns->objects[i], "container_name")))
                        strncpy(c->container_name, v, sizeof(c->container_name) - 1);
                    if ((v = json_get(&conns->objects[i], "veth_host")))
                        strncpy(c->veth_host, v, sizeof(c->veth_host) - 1);
                    if ((v = json_get(&conns->objects[i], "veth_container")))
                        strncpy(c->veth_container, v, sizeof(c->veth_container) - 1);
                    if ((v = json_get(&conns->objects[i], "ip")))
                        strncpy(c->ip, v, sizeof(c->ip) - 1);
                    if ((v = json_get(&conns->objects[i], "ipv6")))
                        strncpy(c->ipv6, v, sizeof(c->ipv6) - 1);

                    /* Parse connected_to array */
                    const char *ct = json_get(&conns->objects[i], "connected_to");
                    c->connected_count = 0;
                    if (ct && ct[0] == '[') {
                        const char *p = ct + 1;
                        while (*p && *p != ']') {
                            while (*p && (*p == ' ' || *p == ',' || *p == '"')) p++;
                            if (*p == ']') break;
                            char id[16];
                            int k = 0;
                            while (*p && *p != '"' && *p != ',' && *p != ']' && k < 15) {
                                id[k++] = *p++;
                            }
                            id[k] = '\0';
                            if (k > 0 && c->connected_count < MAX_CONNECTED_TO) {
                                strncpy(c->connected_to[c->connected_count], id,
                                        sizeof(c->connected_to[0]) - 1);
                                c->connected_count++;
                            }
                            if (*p == '"') p++;
                        }
                    }

                    state->connection_count++;
                }
                json_array_free(conns);
            }
            free(conn_str);
        }
    }

    free(data);
    return 0;
}

int network_write_state(const NetworkState *state) {
    if (!state) return -1;

    mkdir_p(NETWORK_DIR);

    /* Build JSON manually for better control over nested structure */
    size_t buf_size = MAX_BUF * 8;
    char *buf = malloc(buf_size);
    if (!buf) return -1;

    int pos = 0;
    pos += snprintf(buf + pos, buf_size - pos,
        "{\n"
        "  \"bridge\": {\n"
        "    \"name\": \"%s\",\n"
        "    \"ip\": \"%s\",\n"
        "    \"ipv6\": \"%s\",\n"
        "    \"subnet\": \"%s\",\n"
        "    \"ipv6_subnet\": \"%s\"\n"
        "  },\n"
        "  \"next_ip_octet\": %d,\n"
        "  \"next_ipv6_suffix\": %d,\n"
        "  \"connections\": [\n",
        state->bridge_name, state->bridge_ip, state->bridge_ipv6,
        state->subnet, state->ipv6_subnet,
        state->next_ip_octet, state->next_ipv6_suffix);

    for (int i = 0; i < state->connection_count; i++) {
        const NetConnection *c = &state->connections[i];

        if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",\n");

        pos += snprintf(buf + pos, buf_size - pos,
            "    {\n"
            "      \"container_id\": \"%s\",\n"
            "      \"container_name\": \"%s\",\n"
            "      \"veth_host\": \"%s\",\n"
            "      \"veth_container\": \"%s\",\n"
            "      \"ip\": \"%s\",\n"
            "      \"ipv6\": \"%s\",\n"
            "      \"connected_to\": [",
            c->container_id, c->container_name,
            c->veth_host, c->veth_container, c->ip, c->ipv6);

        for (int j = 0; j < c->connected_count; j++) {
            if (j > 0) pos += snprintf(buf + pos, buf_size - pos, ", ");
            pos += snprintf(buf + pos, buf_size - pos,
                            "\"%s\"", c->connected_to[j]);
        }

        pos += snprintf(buf + pos, buf_size - pos,
            "]\n"
            "    }");
    }

    pos += snprintf(buf + pos, buf_size - pos,
        "\n  ]\n"
        "}");

    int result = write_file(NETWORK_STATE, buf);
    free(buf);
    return result;
}

int network_find_connection(const NetworkState *state, const char *container_id) {
    if (!state || !container_id) return -1;
    for (int i = 0; i < state->connection_count; i++) {
        if (strcmp(state->connections[i].container_id, container_id) == 0) {
            return i;
        }
    }
    /* Also search by name */
    for (int i = 0; i < state->connection_count; i++) {
        if (strcmp(state->connections[i].container_name, container_id) == 0) {
            return i;
        }
    }
    return -1;
}

/* ================================================================
 * NETWORK INIT
 * ================================================================ */

int network_init(void) {
    printf("Initializing container network...\n");

    /* Create network directory */
    if (mkdir_p(NETWORK_DIR) != 0) {
        fprintf(stderr, "Error: failed to create network directory\n");
        return -1;
    }

    /* Create bridge interface */
    char cmd[MAX_CMD_LEN];
    int ret, warnings = 0;

    printf("  Creating bridge %s...            ", BRIDGE_NAME);
    fflush(stdout);
    snprintf(cmd, sizeof(cmd),
             "ip link add %s type bridge 2>/dev/null", BRIDGE_NAME);
    ret = system(cmd);

    snprintf(cmd, sizeof(cmd),
             "ip addr add %s dev %s 2>/dev/null", BRIDGE_CIDR, BRIDGE_NAME);
    ret = system(cmd);

    snprintf(cmd, sizeof(cmd),
             "ip -6 addr add %s dev %s 2>/dev/null", BRIDGE_IPV6_CIDR, BRIDGE_NAME);
    ret = system(cmd);

    snprintf(cmd, sizeof(cmd),
             "ip link set %s up 2>/dev/null", BRIDGE_NAME);
    ret = system(cmd);
    if (ret != 0) {
        printf("⚠ (bridge commands may have failed — need sudo)\n");
        warnings++;
    } else {
        printf("✓\n");
    }

    /* Enable IP forwarding */
    printf("  Enabling IP forwarding...        ");
    fflush(stdout);
    if (write_file("/proc/sys/net/ipv4/ip_forward", "1") != 0) {
        printf("⚠ (need sudo)\n");
        warnings++;
    } else {
        printf("✓\n");
    }

    printf("  Enabling IPv6 forwarding...      ");
    fflush(stdout);
    if (write_file("/proc/sys/net/ipv6/conf/all/forwarding", "1") != 0) {
        printf("⚠ (need sudo)\n");
        warnings++;
    } else {
        printf("✓\n");
    }

    /* Add NAT masquerade rule */
    printf("  Adding NAT rule...               ");
    fflush(stdout);
    snprintf(cmd, sizeof(cmd),
             "iptables -t nat -A POSTROUTING -s %s -j MASQUERADE 2>/dev/null",
             BRIDGE_SUBNET);
    ret = system(cmd);

    /* Allow forwarding on bridge */
    snprintf(cmd, sizeof(cmd),
             "iptables -A FORWARD -i %s -o %s -j ACCEPT 2>/dev/null",
             BRIDGE_NAME, BRIDGE_NAME);
    ret = system(cmd);
    snprintf(cmd, sizeof(cmd),
             "ip6tables -A FORWARD -i %s -o %s -j ACCEPT 2>/dev/null",
             BRIDGE_NAME, BRIDGE_NAME);
    ret = system(cmd);
    if (ret != 0) {
        printf("⚠ (iptables may have failed — need sudo)\n");
        warnings++;
    } else {
        printf("✓\n");
    }

    /* Write initial state */
    NetworkState state;
    memset(&state, 0, sizeof(state));
    strncpy(state.bridge_name, BRIDGE_NAME, sizeof(state.bridge_name) - 1);
    strncpy(state.bridge_ip, BRIDGE_IP, sizeof(state.bridge_ip) - 1);
    strncpy(state.bridge_ipv6, BRIDGE_IPV6, sizeof(state.bridge_ipv6) - 1);
    strncpy(state.subnet, BRIDGE_SUBNET, sizeof(state.subnet) - 1);
    strncpy(state.ipv6_subnet, BRIDGE_IPV6_SUBNET, sizeof(state.ipv6_subnet) - 1);
    state.next_ip_octet = 2;
    state.next_ipv6_suffix = 2;
    state.connection_count = 0;

    if (network_write_state(&state) != 0) {
        fprintf(stderr, "Error: failed to write network state\n");
        return -1;
    }

    printf("\nNetwork initialized");
    if (warnings > 0) {
        printf(" with %d warning(s) — run with sudo for full setup", warnings);
    } else {
        printf(" successfully");
    }
    printf(".\nBridge: %s (%s)\n", BRIDGE_NAME, BRIDGE_CIDR);
    printf("Subnet: %s\n", BRIDGE_SUBNET);
    printf("IPv6:   %s (%s)\n", BRIDGE_IPV6, BRIDGE_IPV6_SUBNET);

    return 0;
}

/* ================================================================
 * ASSIGN IP
 * ================================================================ */

int network_assign_ip(const char *container_id, char *ip_out, char *ipv6_out) {
    if (!container_id || !ip_out || !ipv6_out) return -1;

    NetworkState state;
    if (network_read_state(&state) != 0) return -1;

    if (state.next_ip_octet > 254 || state.next_ipv6_suffix > 65535) {
        fprintf(stderr, "Error: IP address pool exhausted\n");
        return -1;
    }

    snprintf(ip_out, 32, "172.18.0.%d", state.next_ip_octet);
    snprintf(ipv6_out, 64, "fd42:18::%x", state.next_ipv6_suffix);
    state.next_ip_octet++;
    state.next_ipv6_suffix++;

    if (network_write_state(&state) != 0) return -1;

    return 0;
}

/* ================================================================
 * SETUP CONTAINER NETWORKING
 * ================================================================ */

int network_setup_container(const char *container_id, pid_t container_pid) {
    if (!container_id) return -1;

    char cmd[MAX_CMD_LEN];
    char ip[32];
    char ipv6[64];

    printf("Setting up network for container %s (PID %d)...\n",
           container_id, container_pid);

    /* Step a: Create veth pair */
    printf("  Creating veth pair...            ");
    fflush(stdout);

    char veth_host[32];
    snprintf(veth_host, sizeof(veth_host), "veth_%s", container_id);

    snprintf(cmd, sizeof(cmd),
             "ip link add %s type veth peer name eth0 netns %d 2>/dev/null",
             veth_host, container_pid);
    if (system(cmd) != 0) {
        printf("✗\n");
        fprintf(stderr, "Error: failed to create veth pair\n");
        return -1;
    }
    printf("✓\n");

    /* Step b: Attach host side to bridge */
    printf("  Attaching to bridge...           ");
    fflush(stdout);

    snprintf(cmd, sizeof(cmd),
             "ip link set %s master %s 2>/dev/null",
             veth_host, BRIDGE_NAME);
    system(cmd);

    snprintf(cmd, sizeof(cmd),
             "ip link set %s up 2>/dev/null", veth_host);
    system(cmd);
    printf("✓\n");

    /* Step c: Configure container side */
    printf("  Configuring container network... ");
    fflush(stdout);

    /* Assign IP */
    if (network_assign_ip(container_id, ip, ipv6) != 0) {
        printf("✗\n");
        return -1;
    }

    /* Set IP inside namespace */
    snprintf(cmd, sizeof(cmd),
             "nsenter --net=/proc/%d/ns/net -- "
             "ip addr add %s/24 dev eth0 2>/dev/null",
             container_pid, ip);
    system(cmd);

    snprintf(cmd, sizeof(cmd),
             "nsenter --net=/proc/%d/ns/net -- "
             "ip -6 addr add %s/64 dev eth0 2>/dev/null",
             container_pid, ipv6);
    system(cmd);

    /* Bring up eth0 */
    snprintf(cmd, sizeof(cmd),
             "nsenter --net=/proc/%d/ns/net -- "
             "ip link set eth0 up 2>/dev/null",
             container_pid);
    system(cmd);

    /* Bring up loopback */
    snprintf(cmd, sizeof(cmd),
             "nsenter --net=/proc/%d/ns/net -- "
             "ip link set lo up 2>/dev/null",
             container_pid);
    system(cmd);

    /* Add default route */
    snprintf(cmd, sizeof(cmd),
             "nsenter --net=/proc/%d/ns/net -- "
             "ip route add default via %s 2>/dev/null",
             container_pid, BRIDGE_IP);
    system(cmd);

    snprintf(cmd, sizeof(cmd),
             "nsenter --net=/proc/%d/ns/net -- "
             "ip -6 route add default via %s dev eth0 2>/dev/null",
             container_pid, BRIDGE_IPV6);
    system(cmd);
    printf("✓\n");

    /* Step d: Save connection to state */
    NetworkState state;
    if (network_read_state(&state) != 0) return -1;

    if (state.connection_count < MAX_CONNECTIONS) {
        NetConnection *c = &state.connections[state.connection_count];
        memset(c, 0, sizeof(NetConnection));
        strncpy(c->container_id, container_id, sizeof(c->container_id) - 1);
        strncpy(c->veth_host, veth_host, sizeof(c->veth_host) - 1);
        strncpy(c->veth_container, "eth0", sizeof(c->veth_container) - 1);
        strncpy(c->ip, ip, sizeof(c->ip) - 1);
        strncpy(c->ipv6, ipv6, sizeof(c->ipv6) - 1);
        c->connected_count = 0;
        state.connection_count++;
    }

    network_write_state(&state);

    printf("\nContainer %s network configured:\n", container_id);
    printf("  IP:   %s/24\n", ip);
    printf("  IPv6: %s/64\n", ipv6);
    printf("  Veth: %s ↔ eth0\n", veth_host);
    printf("  Gateway: %s\n", BRIDGE_IP);
    printf("  IPv6 Gateway: %s\n", BRIDGE_IPV6);

    return 0;
}

/* ================================================================
 * CONNECT TWO CONTAINERS
 * ================================================================ */

int network_connect(const char *source_id, const char *target_id) {
    if (!source_id || !target_id) {
        fprintf(stderr, "Usage: mycontainer network connect <id1> <id2>\n");
        return -1;
    }

    NetworkState state;
    if (network_read_state(&state) != 0) {
        fprintf(stderr, "Error: cannot read network state. Run 'network init' first.\n");
        return -1;
    }

    int src_idx = network_find_connection(&state, source_id);
    int tgt_idx = network_find_connection(&state, target_id);

    if (src_idx == -1) {
        fprintf(stderr, "Error: container %s not found in network\n", source_id);
        return -1;
    }
    if (tgt_idx == -1) {
        fprintf(stderr, "Error: container %s not found in network\n", target_id);
        return -1;
    }

    NetConnection *src = &state.connections[src_idx];
    NetConnection *tgt = &state.connections[tgt_idx];

    /* Check if already connected */
    for (int i = 0; i < src->connected_count; i++) {
        if (strcmp(src->connected_to[i], tgt->container_id) == 0) {
            printf("Containers %s and %s are already connected.\n",
                   source_id, target_id);
            return 0;
        }
    }

    /* Add to connected_to lists (bidirectional) */
    if (src->connected_count < MAX_CONNECTED_TO) {
        strncpy(src->connected_to[src->connected_count],
                tgt->container_id,
                sizeof(src->connected_to[0]) - 1);
        src->connected_count++;
    }

    if (tgt->connected_count < MAX_CONNECTED_TO) {
        strncpy(tgt->connected_to[tgt->connected_count],
                src->container_id,
                sizeof(tgt->connected_to[0]) - 1);
        tgt->connected_count++;
    }

    /* Add iptables ACCEPT rule (they're already on the same bridge,
       but this is explicit) */
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd),
             "iptables -A FORWARD -s %s -d %s -j ACCEPT 2>/dev/null",
             src->ip, tgt->ip);
    system(cmd);
    snprintf(cmd, sizeof(cmd),
             "iptables -A FORWARD -s %s -d %s -j ACCEPT 2>/dev/null",
             tgt->ip, src->ip);
    system(cmd);
    if (strlen(src->ipv6) > 0 && strlen(tgt->ipv6) > 0) {
        snprintf(cmd, sizeof(cmd),
                 "ip6tables -A FORWARD -s %s -d %s -j ACCEPT 2>/dev/null",
                 src->ipv6, tgt->ipv6);
        system(cmd);
        snprintf(cmd, sizeof(cmd),
                 "ip6tables -A FORWARD -s %s -d %s -j ACCEPT 2>/dev/null",
                 tgt->ipv6, src->ipv6);
        system(cmd);
    }

    network_write_state(&state);

    /* Get display names */
    const char *src_name = strlen(src->container_name) > 0
                           ? src->container_name : src->container_id;
    const char *tgt_name = strlen(tgt->container_name) > 0
                           ? tgt->container_name : tgt->container_id;

    printf("Connected %s (%s) ↔ %s (%s)\n",
           src_name, src->ip, tgt_name, tgt->ip);
    if (strlen(src->ipv6) > 0 && strlen(tgt->ipv6) > 0) {
        printf("IPv6:      %s ↔ %s\n", src->ipv6, tgt->ipv6);
    }

    return 0;
}

/* ================================================================
 * DISCONNECT TWO CONTAINERS
 * ================================================================ */

int network_disconnect(const char *source_id, const char *target_id) {
    if (!source_id || !target_id) {
        fprintf(stderr, "Usage: mycontainer network disconnect <id1> <id2>\n");
        return -1;
    }

    NetworkState state;
    if (network_read_state(&state) != 0) return -1;

    int src_idx = network_find_connection(&state, source_id);
    int tgt_idx = network_find_connection(&state, target_id);

    if (src_idx == -1 || tgt_idx == -1) {
        fprintf(stderr, "Error: one or both containers not found in network\n");
        return -1;
    }

    NetConnection *src = &state.connections[src_idx];
    NetConnection *tgt = &state.connections[tgt_idx];

    /* Remove from connected_to lists */
    for (int i = 0; i < src->connected_count; i++) {
        if (strcmp(src->connected_to[i], tgt->container_id) == 0) {
            for (int j = i; j < src->connected_count - 1; j++) {
                strcpy(src->connected_to[j], src->connected_to[j + 1]);
            }
            src->connected_count--;
            break;
        }
    }

    for (int i = 0; i < tgt->connected_count; i++) {
        if (strcmp(tgt->connected_to[i], src->container_id) == 0) {
            for (int j = i; j < tgt->connected_count - 1; j++) {
                strcpy(tgt->connected_to[j], tgt->connected_to[j + 1]);
            }
            tgt->connected_count--;
            break;
        }
    }

    /* Add iptables DROP rule between the two IPs */
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd),
             "iptables -A FORWARD -s %s -d %s -j DROP 2>/dev/null",
             src->ip, tgt->ip);
    system(cmd);
    snprintf(cmd, sizeof(cmd),
             "iptables -A FORWARD -s %s -d %s -j DROP 2>/dev/null",
             tgt->ip, src->ip);
    system(cmd);
    if (strlen(src->ipv6) > 0 && strlen(tgt->ipv6) > 0) {
        snprintf(cmd, sizeof(cmd),
                 "ip6tables -A FORWARD -s %s -d %s -j DROP 2>/dev/null",
                 src->ipv6, tgt->ipv6);
        system(cmd);
        snprintf(cmd, sizeof(cmd),
                 "ip6tables -A FORWARD -s %s -d %s -j DROP 2>/dev/null",
                 tgt->ipv6, src->ipv6);
        system(cmd);
    }

    network_write_state(&state);

    const char *src_name = strlen(src->container_name) > 0
                           ? src->container_name : src->container_id;
    const char *tgt_name = strlen(tgt->container_name) > 0
                           ? tgt->container_name : tgt->container_id;

    printf("Disconnected %s ↔ %s\n", src_name, tgt_name);

    return 0;
}

/* ================================================================
 * LIST NETWORK TOPOLOGY
 * ================================================================ */

int network_list(int json_output) {
    NetworkState state;
    if (network_read_state(&state) != 0) {
        fprintf(stderr, "Error: cannot read network state. Run 'network init' first.\n");
        return -1;
    }

    if (json_output) {
        /* Serialize entire state as JSON */
        size_t buf_size = MAX_BUF * 8;
        char *buf = malloc(buf_size);
        if (!buf) return -1;

        int pos = 0;
        pos += snprintf(buf + pos, buf_size - pos,
            "{\n"
            "  \"bridge\": {\n"
            "    \"name\": \"%s\",\n"
            "    \"ip\": \"%s\",\n"
            "    \"ipv6\": \"%s\",\n"
            "    \"subnet\": \"%s\",\n"
            "    \"ipv6_subnet\": \"%s\"\n"
            "  },\n"
            "  \"containers\": [\n",
            state.bridge_name, state.bridge_ip, state.bridge_ipv6,
            state.subnet, state.ipv6_subnet);

        for (int i = 0; i < state.connection_count; i++) {
            const NetConnection *c = &state.connections[i];
            if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",\n");

            pos += snprintf(buf + pos, buf_size - pos,
                "    {\n"
                "      \"container_id\": \"%s\",\n"
                "      \"name\": \"%s\",\n"
                "      \"ip\": \"%s\",\n"
                "      \"ipv6\": \"%s\",\n"
                "      \"veth\": \"%s\",\n"
                "      \"connected_to\": [",
                c->container_id, c->container_name, c->ip, c->ipv6, c->veth_host);

            for (int j = 0; j < c->connected_count; j++) {
                if (j > 0) pos += snprintf(buf + pos, buf_size - pos, ", ");
                pos += snprintf(buf + pos, buf_size - pos,
                                "\"%s\"", c->connected_to[j]);
            }

            pos += snprintf(buf + pos, buf_size - pos,
                "]\n"
                "    }");
        }

        pos += snprintf(buf + pos, buf_size - pos,
            "\n  ]\n"
            "}");

        printf("%s\n", buf);
        free(buf);
    } else {
        printf("Bridge: %s (%s)\n", state.bridge_name, state.bridge_ip);
        printf("Subnet: %s\n\n", state.subnet);
        printf("IPv6:   %s (%s)\n\n", state.bridge_ipv6, state.ipv6_subnet);

        if (state.connection_count == 0) {
            printf("No containers connected.\n");
            return 0;
        }

        printf("%-12s %-16s %-16s %-22s %-12s %s\n",
               "CONTAINER", "NAME", "IP", "IPV6", "VETH", "CONNECTED TO");
        printf("%-12s %-16s %-16s %-22s %-12s %s\n",
               "---------", "----", "--", "----", "----", "------------");

        for (int i = 0; i < state.connection_count; i++) {
            const NetConnection *c = &state.connections[i];

            /* Build connected_to string */
            char conn_str[256] = "";
            for (int j = 0; j < c->connected_count; j++) {
                if (j > 0) strncat(conn_str, ", ",
                                   sizeof(conn_str) - strlen(conn_str) - 1);
                strncat(conn_str, c->connected_to[j],
                        sizeof(conn_str) - strlen(conn_str) - 1);
            }
            if (c->connected_count == 0) {
                strcpy(conn_str, "(none)");
            }

            const char *name = strlen(c->container_name) > 0
                               ? c->container_name : "-";

            printf("%-12s %-16s %-16s %-22s %-12s %s\n",
                   c->container_id, name, c->ip,
                   strlen(c->ipv6) > 0 ? c->ipv6 : "-",
                   c->veth_host, conn_str);
        }
    }

    return 0;
}

/* ================================================================
 * NETWORK INSPECT
 * ================================================================ */

int network_inspect(const char *container_id, int json_output) {
    if (!container_id) return -1;

    NetworkState state;
    if (network_read_state(&state) != 0) return -1;

    int idx = network_find_connection(&state, container_id);
    if (idx == -1) {
        fprintf(stderr, "Error: container %s not found in network\n", container_id);
        return -1;
    }

    const NetConnection *c = &state.connections[idx];

    if (json_output) {
        printf("{\n");
        printf("  \"container_id\": \"%s\",\n", c->container_id);
        printf("  \"container_name\": \"%s\",\n", c->container_name);
        printf("  \"ip\": \"%s\",\n", c->ip);
        printf("  \"ipv6\": \"%s\",\n", c->ipv6);
        printf("  \"veth_host\": \"%s\",\n", c->veth_host);
        printf("  \"veth_container\": \"%s\",\n", c->veth_container);
        printf("  \"bridge\": \"%s\",\n", state.bridge_name);
        printf("  \"gateway\": \"%s\",\n", state.bridge_ip);
        printf("  \"gateway_ipv6\": \"%s\",\n", state.bridge_ipv6);
        printf("  \"subnet\": \"%s\",\n", state.subnet);
        printf("  \"ipv6_subnet\": \"%s\",\n", state.ipv6_subnet);
        printf("  \"connected_to\": [");
        for (int i = 0; i < c->connected_count; i++) {
            if (i > 0) printf(", ");
            printf("\"%s\"", c->connected_to[i]);
        }
        printf("]\n");
        printf("}\n");
    } else {
        const char *name = strlen(c->container_name) > 0
                           ? c->container_name : c->container_id;
        printf("Network configuration for %s:\n\n", name);
        printf("  Container ID:  %s\n", c->container_id);
        printf("  Name:          %s\n", c->container_name);
        printf("  IP Address:    %s/24\n", c->ip);
        printf("  IPv6 Address:  %s/64\n", strlen(c->ipv6) > 0 ? c->ipv6 : "-");
        printf("  Veth (host):   %s\n", c->veth_host);
        printf("  Veth (ctnr):   %s\n", c->veth_container);
        printf("  Bridge:        %s\n", state.bridge_name);
        printf("  Gateway:       %s\n", state.bridge_ip);
        printf("  IPv6 Gateway:  %s\n", state.bridge_ipv6);
        printf("  Subnet:        %s\n", state.subnet);
        printf("  IPv6 Subnet:   %s\n", state.ipv6_subnet);
        printf("  Connected to:  ");
        if (c->connected_count == 0) {
            printf("(none)\n");
        } else {
            for (int i = 0; i < c->connected_count; i++) {
                if (i > 0) printf(", ");
                printf("%s", c->connected_to[i]);
            }
            printf("\n");
        }
    }

    return 0;
}

/* ================================================================
 * NETWORK CLEANUP (single container)
 * ================================================================ */

int network_cleanup(const char *container_id) {
    if (!container_id) return -1;

    NetworkState state;
    if (network_read_state(&state) != 0) return -1;

    int idx = network_find_connection(&state, container_id);
    if (idx == -1) {
        /* Not in network, nothing to clean up */
        return 0;
    }

    const NetConnection *c = &state.connections[idx];

    /* Delete veth pair (deleting one end removes both) */
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd),
             "ip link delete %s 2>/dev/null", c->veth_host);
    system(cmd);

    /* Remove this container from other containers' connected_to lists */
    for (int i = 0; i < state.connection_count; i++) {
        if (i == idx) continue;
        NetConnection *other = &state.connections[i];
        for (int j = 0; j < other->connected_count; j++) {
            if (strcmp(other->connected_to[j], c->container_id) == 0) {
                for (int k = j; k < other->connected_count - 1; k++) {
                    strcpy(other->connected_to[k], other->connected_to[k + 1]);
                }
                other->connected_count--;
                break;
            }
        }
    }

    /* Remove connection from state */
    for (int i = idx; i < state.connection_count - 1; i++) {
        memcpy(&state.connections[i], &state.connections[i + 1],
               sizeof(NetConnection));
    }
    state.connection_count--;

    /* Note: We don't release the IP back to the pool to avoid conflicts.
     * IPs are only recycled on full network destroy. */

    network_write_state(&state);

    printf("Cleaned up network for container %s\n", container_id);
    return 0;
}

/* ================================================================
 * NETWORK DESTROY
 * ================================================================ */

int network_destroy(void) {
    printf("Destroying container network...\n");

    char cmd[MAX_CMD_LEN];

    /* Delete bridge (this also removes attached veths) */
    printf("  Deleting bridge %s...       ", BRIDGE_NAME);
    fflush(stdout);
    snprintf(cmd, sizeof(cmd),
             "ip link set %s down 2>/dev/null", BRIDGE_NAME);
    system(cmd);
    snprintf(cmd, sizeof(cmd),
             "ip link delete %s 2>/dev/null", BRIDGE_NAME);
    system(cmd);
    printf("✓\n");

    /* Remove iptables rules */
    printf("  Removing iptables rules...       ");
    fflush(stdout);
    snprintf(cmd, sizeof(cmd),
             "iptables -t nat -D POSTROUTING -s %s -j MASQUERADE 2>/dev/null",
             BRIDGE_SUBNET);
    system(cmd);
    snprintf(cmd, sizeof(cmd),
             "iptables -D FORWARD -i %s -o %s -j ACCEPT 2>/dev/null",
             BRIDGE_NAME, BRIDGE_NAME);
    system(cmd);
    snprintf(cmd, sizeof(cmd),
             "ip6tables -D FORWARD -i %s -o %s -j ACCEPT 2>/dev/null",
             BRIDGE_NAME, BRIDGE_NAME);
    system(cmd);
    printf("✓\n");

    /* Clean up any remaining veth interfaces */
    NetworkState state;
    if (network_read_state(&state) == 0) {
        for (int i = 0; i < state.connection_count; i++) {
            snprintf(cmd, sizeof(cmd),
                     "ip link delete %s 2>/dev/null",
                     state.connections[i].veth_host);
            system(cmd);
        }
    }

    /* Clear state file */
    printf("  Clearing state...                ");
    fflush(stdout);
    rmdir_recursive(NETWORK_DIR);
    printf("✓\n");

    printf("\nNetwork destroyed.\n");
    return 0;
}
