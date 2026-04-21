const express = require('express');

const { bridge } = require('../lib/simulator');
const { asyncHandler, run } = require('../lib/http');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await bridge.commit.containers());
}));

router.get('/:id', asyncHandler(async (req, res) => {
    res.json(await run(['volume', 'ls', req.params.id, '--json']));
}));

module.exports = router;
