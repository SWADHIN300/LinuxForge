const fs = require('fs');
const path = require('path');

const DATA_DIR = path.join(__dirname, '..', 'data');

function ensureDir(targetPath) {
    fs.mkdirSync(targetPath, { recursive: true });
}

function dataPath(...segments) {
    ensureDir(DATA_DIR);
    return path.join(DATA_DIR, ...segments);
}

function readJson(fileName, fallback) {
    const target = dataPath(fileName);

    if (!fs.existsSync(target)) {
        return fallback;
    }

    try {
        return JSON.parse(fs.readFileSync(target, 'utf8'));
    } catch {
        return fallback;
    }
}

function writeJson(fileName, value) {
    const target = dataPath(fileName);
    fs.writeFileSync(target, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
    return value;
}

module.exports = {
    DATA_DIR,
    dataPath,
    ensureDir,
    readJson,
    writeJson,
};
