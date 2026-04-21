const fs = require('fs');
const path = require('path');

const express = require('express');
const multer = require('multer');

const { ensureDir } = require('../lib/data');
const {
    EXPORTS_DIR,
    UPLOADS_DIR,
    asyncHandler,
    ensureExtension,
    requireString,
    run,
    sanitizeFileName,
    toSimulatorPath,
} = require('../lib/http');

ensureDir(EXPORTS_DIR);
ensureDir(UPLOADS_DIR);

const upload = multer({ dest: UPLOADS_DIR });
const router = express.Router();

router.get('/files', asyncHandler(async (_req, res) => {
    const files = fs.readdirSync(EXPORTS_DIR, { withFileTypes: true })
        .filter((entry) => entry.isFile())
        .map((entry) => entry.name)
        .sort()
        .map((name) => ({
            name,
            url: `/exports/${name}`,
        }));

    res.json(files);
}));

router.post('/containers/:id', asyncHandler(async (req, res) => {
    const baseName = sanitizeFileName(
        req.body && req.body.fileName,
        `${req.params.id}-${Date.now()}`
    );
    const fileName = ensureExtension(baseName, '.tar.gz');
    const outputPath = path.join(EXPORTS_DIR, fileName);
    const result = await run(['export', req.params.id, toSimulatorPath(outputPath), '--json']);

    res.status(201).json({
        ...result,
        download_url: `/exports/${fileName}`,
    });
}));

router.post('/images/import', asyncHandler(async (req, res) => {
    const tarPath = requireString(req.body && req.body.tarPath, 'tarPath');
    const image = requireString(req.body && req.body.image, 'image');

    res.status(201).json(await run(['import', toSimulatorPath(tarPath), image, '--json']));
}));

router.post('/images/import-upload', upload.single('archive'), asyncHandler(async (req, res) => {
    if (!req.file) {
        const err = new Error('archive is required');
        err.status = 400;
        throw err;
    }

    const image = requireString(req.body && req.body.image, 'image');
    res.status(201).json(await run(['import', toSimulatorPath(req.file.path), image, '--json']));
}));

module.exports = router;
