CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LIBS =

SRCS = src/main.c src/container.c \
       src/cgroups.c src/fs.c \
       src/registry.c src/commit.c \
       src/network.c src/utils.c \
       src/logs.c src/stats.c \
       src/health.c src/volume.c \
       src/stack.c src/security.c \
       src/dns.c src/export.c \
       src/checkpoint.c

TARGET = mycontainer_linux
SMOKE_ID_FILE = .test-smoke-id
SMOKE_CHECKPOINT_DIR = checkpoints/test-smoke

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	rm -f $(TARGET) $(SMOKE_ID_FILE)

clean-all: clean
	rm -rf registry/ containers/ network/ logs/ stats/ checkpoints/

test-smoke-setup: $(TARGET)
	@set -eu; \
	rm -f $(SMOKE_ID_FILE); \
	rm -rf $(SMOKE_CHECKPOINT_DIR); \
	TMP_DIR=$$(mktemp -d); \
	mkdir -p $$TMP_DIR/rootfs/bin $$TMP_DIR/rootfs/etc; \
	printf '%s\n' '#!/bin/sh' 'exit 0' > $$TMP_DIR/rootfs/bin/sh; \
	printf '%s\n' 'Smoke test rootfs' > $$TMP_DIR/rootfs/etc/motd; \
	chmod +x $$TMP_DIR/rootfs/bin/sh; \
	tar -czf $$TMP_DIR/rootfs.tar.gz -C $$TMP_DIR/rootfs .; \
	./$(TARGET) image push smoke:test $$TMP_DIR/rootfs.tar.gz >/dev/null; \
	./$(TARGET) run --name=smoke-test --image=smoke:test --healthcheck='echo ok' --json /bin/sh > $$TMP_DIR/run.json; \
	python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['id'])" $$TMP_DIR/run.json > $(SMOKE_ID_FILE); \
	rm -rf $$TMP_DIR; \
	test -s $(SMOKE_ID_FILE); \
	echo "Smoke container: $$(cat $(SMOKE_ID_FILE))"

test-logs: test-smoke-setup
	./$(TARGET) logs $$(cat $(SMOKE_ID_FILE)) --tail=10

test-stats: test-smoke-setup
	./$(TARGET) stats $$(cat $(SMOKE_ID_FILE)) --json

test-health: test-smoke-setup
	./$(TARGET) health $$(cat $(SMOKE_ID_FILE)) --run --json

test-stack: $(TARGET)
	./$(TARGET) stack up stacks/test.json
	./$(TARGET) stack status test-stack
	./$(TARGET) stack down test-stack

test-dns: $(TARGET)
	./$(TARGET) dns ls

test-network: $(TARGET)
	bash ./test_network.sh

test-checkpoint: test-smoke-setup
	rm -rf $(SMOKE_CHECKPOINT_DIR)
	./$(TARGET) checkpoint $$(cat $(SMOKE_ID_FILE)) ./$(SMOKE_CHECKPOINT_DIR)
	./$(TARGET) restore ./$(SMOKE_CHECKPOINT_DIR) restored-smoke

test-registry: $(TARGET)
	./$(TARGET) image ls --json

test-commit: $(TARGET)
	./$(TARGET) commit ls --json

test-all: test-registry test-commit test-logs test-stats test-health test-stack test-dns test-checkpoint

demo-run: $(TARGET)
	./$(TARGET) run --name=demo-01 /bin/sh

demo-full: $(TARGET)
	mkdir -p /tmp/demo_rootfs/bin /tmp/demo_rootfs/etc
	echo '#!/bin/sh' > /tmp/demo_rootfs/bin/sh
	echo 'Hello from container!' > /tmp/demo_rootfs/etc/motd
	chmod +x /tmp/demo_rootfs/bin/sh
	tar -czf /tmp/demo_rootfs.tar.gz -C /tmp/demo_rootfs .
	./$(TARGET) image push alpine:3.18 /tmp/demo_rootfs.tar.gz
	./$(TARGET) run --name=web-01 --image=alpine:3.18 /bin/sh
	./$(TARGET) image ls
	./$(TARGET) commit ls
	rm -rf /tmp/demo_rootfs /tmp/demo_rootfs.tar.gz

.PHONY: all clean clean-all test-smoke-setup test-logs test-stats test-health test-stack \
        test-dns test-network test-checkpoint test-registry test-commit test-all \
        demo-run demo-full
