const path = require('path');

const { bridge } = require('./simulator');

const PROJECT_DIR = path.resolve(__dirname, '..', '..', '..');
const BACKEND_DIR = path.join(PROJECT_DIR, 'backend');
const EXPORTS_DIR = path.join(BACKEND_DIR, 'exports');
const UPLOADS_DIR = path.join(BACKEND_DIR, 'data', 'uploads');

function asyncHandler(handler) {
    return (req, res, next) => {
        Promise.resolve(handler(req, res, next)).catch(next);
    };
}

function normalizeResult(result) {
    if (result && result.raw) {
        return { output: result.output || '' };
    }

    return result;
}

async function run(argv) {
    return normalizeResult(await bridge.run(argv));
}

function requireString(value, fieldName) {
    if (typeof value !== 'string' || value.trim().length === 0) {
        const err = new Error(`${fieldName} is required`);
        err.status = 400;
        throw err;
    }

    return value.trim();
}

function sanitizeFileName(value, fallback = 'artifact') {
    const safe = String(value || fallback)
        .replace(/[^a-zA-Z0-9._-]+/g, '-')
        .replace(/^-+|-+$/g, '');

    return safe || fallback;
}

function ensureExtension(fileName, extension) {
    if (fileName.endsWith(extension)) {
        return fileName;
    }

    return `${fileName}${extension}`;
}

function toSimulatorPath(targetPath) {
    if (typeof targetPath !== 'string' || targetPath.length === 0) {
        return targetPath;
    }

    if (!path.isAbsolute(targetPath)) {
        return targetPath.replace(/\\/g, '/');
    }

    const relative = path.relative(PROJECT_DIR, targetPath);

    if (!relative.startsWith('..') && !path.isAbsolute(relative)) {
        return relative.replace(/\\/g, '/');
    }

    if (process.platform === 'win32') {
        const resolved = path.resolve(targetPath).replace(/\\/g, '/');
        const driveMatch = resolved.match(/^([A-Za-z]):(\/.*)$/);

        if (driveMatch) {
            return `/mnt/${driveMatch[1].toLowerCase()}${driveMatch[2]}`;
        }
    }

    return targetPath.replace(/\\/g, '/');
}

module.exports = {
    PROJECT_DIR,
    BACKEND_DIR,
    EXPORTS_DIR,
    UPLOADS_DIR,
    asyncHandler,
    run,
    requireString,
    sanitizeFileName,
    ensureExtension,
    toSimulatorPath,
};
