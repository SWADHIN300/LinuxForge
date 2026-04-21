const path = require('path');

const bridge = require(path.join(__dirname, '..', '..', '..', 'simulatorBridge'));

async function callOrNull(fn, fallback = null) {
    try {
        return await fn();
    } catch {
        return fallback;
    }
}

module.exports = {
    bridge,
    callOrNull,
};
