/*
 * simulatorBridge.js — Node.js bridge to mycontainer C binary
 *
 * Supports a Unix-socket request path on Linux/WSL hosts and
 * falls back to direct binary execution when the socket server
 * is unavailable or the caller is running on Windows.
 */

const { execFile } = require('child_process');
const fs = require('fs');
const net = require('net');
const path = require('path');

const PROJECT_DIR = __dirname;
const LINUX_BINARY_NAME = process.env.MYCONTAINER_BINARY ||
    (fs.existsSync(path.join(PROJECT_DIR, 'mycontainer_linux')) ? 'mycontainer_linux' : 'mycontainer');
const BINARY = path.join(PROJECT_DIR, LINUX_BINARY_NAME);
const IS_WINDOWS = process.platform === 'win32';
const SOCKET_PATH = process.env.MYCONTAINER_SOCKET || '/tmp/mycontainer.sock';

function toWslPath(targetPath) {
    const resolved = path.resolve(targetPath).replace(/\\/g, '/');
    const driveMatch = resolved.match(/^([A-Za-z]):(\/.*)$/);

    if (!driveMatch) {
        return resolved;
    }

    return `/mnt/${driveMatch[1].toLowerCase()}${driveMatch[2]}`;
}

function shellEscape(arg) {
    return `'${String(arg).replace(/'/g, `'\\''`)}'`;
}

function splitArgs(args) {
    if (Array.isArray(args)) {
        return args.map((arg) => String(arg));
    }

    if (typeof args !== 'string' || args.trim() === '') {
        return [];
    }

    const tokens = [];
    let current = '';
    let quote = null;
    let escaping = false;

    for (const ch of args) {
        if (escaping) {
            current += ch;
            escaping = false;
            continue;
        }

        if (ch === '\\' && quote !== "'") {
            escaping = true;
            continue;
        }

        if ((ch === '"' || ch === "'")) {
            if (quote === ch) {
                quote = null;
                continue;
            }
            if (quote === null) {
                quote = ch;
                continue;
            }
        }

        if (/\s/.test(ch) && quote === null) {
            if (current) {
                tokens.push(current);
                current = '';
            }
            continue;
        }

        current += ch;
    }

    if (escaping || quote !== null) {
        throw new Error('Unable to parse command arguments');
    }

    if (current) {
        tokens.push(current);
    }

    return tokens;
}

function normalizePathArg(targetPath) {
    if (typeof targetPath !== 'string' || targetPath.length === 0) {
        return targetPath;
    }

    if (IS_WINDOWS) {
        if (path.isAbsolute(targetPath)) {
            return toWslPath(targetPath);
        }
        return targetPath.replace(/\\/g, '/');
    }

    return targetPath;
}

function executeDirect(argv) {
    return new Promise((resolve, reject) => {
        const options = {
            cwd: PROJECT_DIR,
            maxBuffer: 1024 * 1024,
            timeout: 30000,
            windowsHide: true,
        };

        if (IS_WINDOWS) {
            const linuxProjectDir = toWslPath(PROJECT_DIR);
            const command = [
                `cd ${shellEscape(linuxProjectDir)}`,
                `./${LINUX_BINARY_NAME} ${argv.map(shellEscape).join(' ')}`
            ].join(' && ');

            execFile('wsl', ['bash', '-lc', command], options, (err, stdout, stderr) => {
                if (err) {
                    return reject({
                        error: err.message,
                        stderr: stderr.trim(),
                        code: err.code,
                        hint: 'Run this bridge with a working WSL installation because mycontainer is a Linux binary.',
                    });
                }
                resolve(stdout.trim());
            });
            return;
        }

        execFile(BINARY, argv, options, (err, stdout, stderr) => {
            if (err) {
                return reject({
                    error: err.message,
                    stderr: stderr.trim(),
                    code: err.code,
                });
            }
            resolve(stdout.trim());
        });
    });
}

function shouldUseSocket() {
    return !IS_WINDOWS && process.env.MYCONTAINER_NO_SOCKET !== '1';
}

function shouldFallbackToDirect(err) {
    return Boolean(err) && (
        err.code === 'ENOENT' ||
        err.code === 'ECONNREFUSED' ||
        err.code === 'EPIPE' ||
        err.code === 'socket_unavailable'
    );
}

function executeViaSocket(argv) {
    return new Promise((resolve, reject) => {
        const client = net.createConnection(SOCKET_PATH);
        let buffer = '';
        let settled = false;

        const fail = (err) => {
            if (settled) return;
            settled = true;
            client.destroy();
            reject(err);
        };

        client.setEncoding('utf8');

        client.on('connect', () => {
            client.write(`${JSON.stringify({ argv })}\n`);
        });

        client.on('data', (chunk) => {
            buffer += chunk;
            const newlineIndex = buffer.indexOf('\n');
            if (newlineIndex === -1) {
                return;
            }

            const line = buffer.slice(0, newlineIndex);
            buffer = buffer.slice(newlineIndex + 1);

            try {
                const message = JSON.parse(line);
                settled = true;
                client.end();
                if (message.ok) {
                    resolve((message.stdout || '').trim());
                } else {
                    reject({
                        error: message.error || 'Socket bridge request failed',
                        stderr: (message.stderr || '').trim(),
                        code: message.code || 1,
                    });
                }
            } catch (err) {
                fail({
                    error: err.message,
                    code: 'socket_unavailable',
                });
            }
        });

        client.on('error', (err) => {
            fail({
                error: err.message,
                code: err.code || 'socket_unavailable',
            });
        });

        client.on('end', () => {
            if (!settled) {
                fail({
                    error: 'Socket bridge closed without a response',
                    code: 'socket_unavailable',
                });
            }
        });
    });
}

function execute(argv) {
    if (shouldUseSocket()) {
        return executeViaSocket(argv).catch((err) => {
            if (shouldFallbackToDirect(err)) {
                return executeDirect(argv);
            }
            throw err;
        });
    }

    return executeDirect(argv);
}

/**
 * Execute a mycontainer command and return the result.
 * Accepts either a shell-like string or an argv array.
 * @param {string|string[]} args
 * @returns {Promise<Object>} Parsed JSON output or raw output object
 */
function run(args) {
    const argv = splitArgs(args);

    return execute(argv).then((stdout) => {
        if (!stdout) {
            return { output: '', raw: true };
        }

        try {
            return JSON.parse(stdout);
        } catch {
            return { output: stdout, raw: true };
        }
    });
}

const registry = {
    list: () => run(['image', 'ls', '--json']),
    push: (name, tag, rootfsPath) =>
        run(['image', 'push', `${name}:${tag}`, normalizePathArg(rootfsPath)]),
    build: (contextDir, name, tag, options = {}) => {
        const argv = ['image', 'build', normalizePathArg(contextDir), `${name}:${tag}`];
        if (options.node) argv.push('--node');
        if (options.cmd) argv.push(`--cmd=${options.cmd}`);
        argv.push('--json');
        return run(argv);
    },
    pull: (name, tag) => run(['image', 'pull', `${name}:${tag}`]),
    remove: (name, tag) => run(['image', 'rm', `${name}:${tag}`]),
    inspect: (name, tag) => run(['image', 'inspect', `${name}:${tag}`, '--json']),
    sign: (name, tag, key) => {
        const argv = ['image', 'sign', `${name}:${tag}`, '--json'];
        if (key) argv.push(`--key=${key}`);
        return run(argv);
    },
    verify: (name, tag, key) => {
        const argv = ['image', 'verify', `${name}:${tag}`, '--json'];
        if (key) argv.push(`--key=${key}`);
        return run(argv);
    },
};

const commit = {
    containers: () => run(['commit', 'ls', '--json']),
    create: (id, name, tag, description = '') => {
        const argv = ['commit', id, `${name}:${tag}`];
        if (description) {
            argv.push(`--description=${description}`);
        }
        argv.push('--json');
        return run(argv);
    },
    history: () => run(['commit', 'history', '--json']),
};

const containers = {
    run: ({ name, image, rootless = true, privileged = false, cpuset, command }) => {
        const argv = ['run'];
        const commandArgs = [];
        if (name) argv.push(`--name=${name}`);
        if (image) argv.push(`--image=${image}`);
        if (privileged) {
            argv.push('--privileged');
        } else if (rootless) {
            argv.push('--rootless');
        }
        if (cpuset) argv.push(`--cpuset=${cpuset}`);
        if (Array.isArray(command)) {
            commandArgs.push(...command.map(String));
        } else if (typeof command === 'string' && command.length > 0) {
            commandArgs.push(...splitArgs(command));
        }
        argv.push('--json');
        argv.push(...commandArgs);
        return run(argv);
    },
};

const network = {
    init: () => run(['network', 'init']),
    topology: () => run(['network', 'ls', '--json']),
    connect: (sourceId, targetId) =>
        run(['network', 'connect', sourceId, targetId]),
    disconnect: (sourceId, targetId) =>
        run(['network', 'disconnect', sourceId, targetId]),
    inspect: (id) => run(['network', 'inspect', id, '--json']),
    destroy: () => run(['network', 'destroy']),
};

module.exports = {
    registry,
    commit,
    containers,
    network,
    run,
    executeDirect,
    BINARY,
    IS_WINDOWS,
    SOCKET_PATH,
};
