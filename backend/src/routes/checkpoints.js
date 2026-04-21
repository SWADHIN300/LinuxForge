const express = require('express');

const { asyncHandler, requireString, run, sanitizeFileName } = require('../lib/http');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await run(['checkpoint', 'ls', '--json']));
}));

router.post('/restore', asyncHandler(async (req, res) => {
    const checkpointDir = requireString(req.body && req.body.checkpointDir, 'checkpointDir');
    const name = requireString(req.body && req.body.name, 'name');

    res.status(201).json(await run(['restore', checkpointDir, name, '--json']));
}));

router.post('/:id', asyncHandler(async (req, res) => {
    const checkpointDir = (req.body && req.body.checkpointDir)
        ? requireString(req.body.checkpointDir, 'checkpointDir')
        : `checkpoints/${sanitizeFileName(req.params.id, 'checkpoint')}`;

    res.status(201).json(await run(['checkpoint', req.params.id, checkpointDir, '--json']));
}));

module.exports = router;
