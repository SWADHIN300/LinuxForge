const express = require('express');

const { asyncHandler, requireString, run } = require('../lib/http');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await run(['stack', 'ls', '--json']));
}));

router.post('/up', asyncHandler(async (req, res) => {
    const file = requireString(req.body && req.body.file, 'file');
    res.status(201).json(await run(['stack', 'up', file, '--json']));
}));

router.post('/down', asyncHandler(async (req, res) => {
    const target = requireString(
        (req.body && (req.body.name || req.body.file || req.body.target)),
        'name or file'
    );
    res.json(await run(['stack', 'down', target, '--json']));
}));

router.get('/:name', asyncHandler(async (req, res) => {
    res.json(await run(['stack', 'status', req.params.name, '--json']));
}));

module.exports = router;
