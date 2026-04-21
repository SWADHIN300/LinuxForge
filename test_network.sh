#!/bin/bash
cd /mnt/c/Users/swadh/LinuXForge/mycontainer

# First init the network to create the directory
./mycontainer network init 2>/dev/null

# Now write test state with 2 containers
cat > network/state.json << 'EOF'
{
  "bridge": {
    "name": "mycontainer0",
    "ip": "172.18.0.1",
    "ipv6": "fd42:18::1",
    "subnet": "172.18.0.0/24",
    "ipv6_subnet": "fd42:18::/64"
  },
  "next_ip_octet": 4,
  "next_ipv6_suffix": 4,
  "connections": [
    {
      "container_id": "a3f9c2b1",
      "container_name": "web-01",
      "veth_host": "veth_a3f9c2b1",
      "veth_container": "eth0",
      "ip": "172.18.0.2",
      "ipv6": "fd42:18::2",
      "connected_to": []
    },
    {
      "container_id": "b7e3d1c2",
      "container_name": "db-01",
      "veth_host": "veth_b7e3d1c2",
      "veth_container": "eth0",
      "ip": "172.18.0.3",
      "ipv6": "fd42:18::3",
      "connected_to": []
    }
  ]
}
EOF

echo "=== TEST 3: NETWORKING ==="
echo ""
echo "--- network ls ---"
./mycontainer network ls
echo ""

echo "--- network connect web-01 db-01 ---"
./mycontainer network connect a3f9c2b1 b7e3d1c2 2>/dev/null
echo ""

echo "--- network ls after connect ---"
./mycontainer network ls
echo ""

echo "--- network inspect web-01 ---"
./mycontainer network inspect a3f9c2b1
echo ""

echo "--- network inspect db-01 --json ---"
./mycontainer network inspect b7e3d1c2 --json
echo ""

echo "--- network disconnect ---"
./mycontainer network disconnect a3f9c2b1 b7e3d1c2 2>/dev/null
echo ""

echo "--- network ls after disconnect ---"
./mycontainer network ls
echo ""

echo "--- network destroy ---"
./mycontainer network destroy 2>/dev/null
echo ""

echo "=== ALL NETWORK TESTS COMPLETE ==="
