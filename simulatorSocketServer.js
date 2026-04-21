/*
 * simulatorSocketServer.js — Unix socket bridge for mycontainer
 *
 * Usage:
 *   node simulatorSocketServer.js
 *
 * Request format (newline-delimited JSON):
 *   { "argv": ["image", "ls", "--json"] }
 */

const fs = require('fs');
const net = require('net');
const { executeDirect, SOCKET_PATH } = require('./simulatorBridge');

if (process.platform === 'win32') {
    console.error('simulatorSocketServer.js requires a Unix-socket-capable host.');
    process.exit(1);
}

function cleanupAndExit(code = 0) {
    try {
        if (fs.existsSync(SOCKET_PATH)) {
            fs.unlinkSync(SOCKET_PATH);
        }
    } catch {
        // best effort cleanup
    }
    process.exit(code);
}

try {
    if (fs.existsSync(SOCKET_PATH)) {
        fs.unlinkSync(SOCKET_PATH);
    }
} catch (err) {
    console.error(`Failed to prepare socket ${SOCKET_PATH}: ${err.message}`);
    process.exit(1);
}

const server = net.createServer((socket) => {
    let buffer = '';

    socket.setEncoding('utf8');

    socket.on('data', async (chunk) => {
        buffer += chunk;

        let newlineIndex = buffer.indexOf('\n');
        while (newlineIndex !== -1) {
            const line = buffer.slice(0, newlineIndex).trim();
            buffer = buffer.slice(newlineIndex + 1);
            newlineIndex = buffer.indexOf('\n');

            if (!line) {
                continue;
            }

            let request;
            try {
                request = JSON.parse(line);
            } catch (err) {
                socket.write(`${JSON.stringify({ ok: false, error: err.message, code: 400 })}\n`);
                continue;
            }

            if (!request || !Array.isArray(request.argv)) {
                socket.write(`${JSON.stringify({ ok: false, error: 'argv array is required', code: 400 })}\n`);
                continue;
            }

            try {
                const stdout = await executeDirect(request.argv);
                socket.write(`${JSON.stringify({ ok: true, stdout })}\n`);
            } catch (err) {
                socket.write(`${JSON.stringify({
                    ok: false,
                    error: err.error || err.message || 'Bridge execution failed',
                    stderr: err.stderr || '',
                    code: err.code || 1,
                })}\n`);
            }
        }
    });
});

server.listen(SOCKET_PATH, () => {
    fs.chmodSync(SOCKET_PATH, 0o600);
    console.log(`mycontainer socket bridge listening on ${SOCKET_PATH}`);
});

process.on('SIGINT', () => cleanupAndExit(0));
process.on('SIGTERM', () => cleanupAndExit(0));
