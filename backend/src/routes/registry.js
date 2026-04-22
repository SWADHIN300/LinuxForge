const express = require('express');
const multer = require('multer');

const { ensureDir } = require('../lib/data');
const {
    UPLOADS_DIR,
    asyncHandler,
    requireString,
    run,
    toSimulatorPath,
} = require('../lib/http');
const { bridge } = require('../lib/simulator');

ensureDir(UPLOADS_DIR);

const upload = multer({ dest: UPLOADS_DIR });
const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await bridge.registry.list());
}));

router.post('/build', asyncHandler(async (req, res) => {
    const image = requireString(req.body && req.body.image, 'image');
    const contextDir = requireString(req.body && req.body.contextDir, 'contextDir');
    const [name, tag = 'latest'] = image.split(':');

    if (!name || !tag) {
        const err = new Error('image must use the format name:tag');
        err.status = 400;
        throw err;
    }

    res.status(201).json(await bridge.registry.build(contextDir, name, tag, {
        node: Boolean(req.body && req.body.node),
        cmd: req.body && req.body.cmd ? String(req.body.cmd).trim() : '',
    }));
}));

router.post('/import', asyncHandler(async (req, res) => {
    const tarPath = requireString(req.body && req.body.tarPath, 'tarPath');
    const image = requireString(req.body && req.body.image, 'image');

    res.status(201).json(await run(['import', toSimulatorPath(tarPath), image, '--json']));
}));

router.post('/import-upload', upload.single('archive'), asyncHandler(async (req, res) => {
    if (!req.file) {
        const err = new Error('archive is required');
        err.status = 400;
        throw err;
    }

    const image = requireString(req.body && req.body.image, 'image');
    res.status(201).json(await run(['import', toSimulatorPath(req.file.path), image, '--json']));
}));

module.exports = router;
