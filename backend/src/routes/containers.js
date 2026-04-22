const express = require('express');

const { asyncHandler, requireString } = require('../lib/http');
const { bridge } = require('../lib/simulator');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await bridge.commit.containers());
}));

router.post('/run', asyncHandler(async (req, res) => {
    const image = requireString(req.body && req.body.image, 'image');
    const result = await bridge.containers.run({
        name: req.body && req.body.name ? String(req.body.name).trim() : '',
        image,
        rootless: req.body && req.body.rootless !== false,
        privileged: Boolean(req.body && req.body.privileged),
        cpuset: req.body && req.body.cpuset ? String(req.body.cpuset).trim() : '',
        command: req.body && req.body.command ? String(req.body.command) : '',
    });

    res.status(201).json(result);
}));

module.exports = router;
