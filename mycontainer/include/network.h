/*
 * network.h — Container-to-container networking
 *
 * Manages a Linux bridge (mycontainer0), veth pairs,
 * IP assignment, and iptables rules for inter-container
 * communication via network namespaces.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "utils.h"
#include <sys/types.h>

/* ---- Constants ---- */
#define NETWORK_DIR         "network"
#define NETWORK_STATE       "network/state.json"
#define BRIDGE_NAME         "mycontainer0"
#define BRIDGE_IP           "172.18.0.1"
#define BRIDGE_SUBNET       "172.18.0.0/24"
#define BRIDGE_CIDR         "172.18.0.1/24"
#define BRIDGE_IPV6         "fd42:18::1"
#define BRIDGE_IPV6_SUBNET  "fd42:18::/64"
#define BRIDGE_IPV6_CIDR    "fd42:18::1/64"
#define MAX_CONNECTIONS     64
#define MAX_CONNECTED_TO    16

/* ---- Network Connection ---- */
typedef struct {
    char container_id[16];
    char container_name[64];
    char veth_host[32];      /* "veth_a3f9c2" */
    char veth_container[16]; /* "eth0" */
    char ip[32];             /* "172.18.0.2" */
    char ipv6[64];           /* "fd42:18::2" */
    char connected_to[MAX_CONNECTED_TO][16]; /* container IDs */
    int  connected_count;
} NetConnection;

/* ---- Network State ---- */
typedef struct {
    char bridge_name[32];
    char bridge_ip[32];
    char bridge_ipv6[64];
    char subnet[32];
    char ipv6_subnet[64];
    int  next_ip_octet;
    int  next_ipv6_suffix;
    NetConnection connections[MAX_CONNECTIONS];
    int connection_count;
} NetworkState;

/* ---- Core Network Functions ---- */

/*
 * Initialize the network bridge.
 * Creates mycontainer0 bridge, enables IP forwarding,
 * adds NAT masquerade rule, creates network/state.json.
 * Returns 0 on success, -1 on error.
 */
int network_init(void);

/*
 * Assign a unique IP address to a container.
 * Reads and increments next_ip_octet in state.json.
 * Writes the assigned IP to ip_out (must be >= 32 chars).
 * Returns 0 on success.
 */
int network_assign_ip(const char *container_id, char *ip_out, char *ipv6_out);

/*
 * Set up networking for a container.
 * Creates veth pair, attaches to bridge, configures
 * IP inside the container's network namespace.
 * Returns 0 on success.
 */
int network_setup_container(const char *container_id, pid_t container_pid);

/*
 * Connect two containers (allow traffic between them).
 * Updates connected_to lists and adds iptables ACCEPT rule.
 * Returns 0 on success.
 */
int network_connect(const char *source_id, const char *target_id);

/*
 * Disconnect two containers (block traffic between them).
 * Removes from connected_to and adds iptables DROP rule.
 * Returns 0 on success.
 */
int network_disconnect(const char *source_id, const char *target_id);

/*
 * List current network topology.
 * If json_output is non-zero, print JSON.
 * Returns 0 on success.
 */
int network_list(int json_output);

/*
 * Clean up networking for a container (on exit).
 * Deletes veth pair, removes from state.json, releases IP.
 * Returns 0 on success.
 */
int network_cleanup(const char *container_id);

/*
 * Destroy the entire network setup.
 * Deletes bridge, removes iptables rules, clears state.
 * Returns 0 on success.
 */
int network_destroy(void);

/*
 * Inspect a single container's network configuration.
 * If json_output is non-zero, print JSON.
 * Returns 0 on success.
 */
int network_inspect(const char *container_id, int json_output);

/* ---- State I/O ---- */

/* Read network state from file. Returns 0 on success. */
int network_read_state(NetworkState *state);

/* Write network state to file. Returns 0 on success. */
int network_write_state(const NetworkState *state);

/* Find a connection by container ID. Returns index or -1. */
int network_find_connection(const NetworkState *state, const char *container_id);

#endif /* NETWORK_H */
