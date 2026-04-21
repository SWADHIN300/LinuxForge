const express = require('express');

const { asyncHandler, run } = require('../lib/http');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await run(['health', '--all', '--json']));
}));

router.post('/:id/run', asyncHandler(async (req, res) => {
    res.json(await run(['health', req.params.id, '--run', '--json']));
}));

router.get('/:id', asyncHandler(async (req, res) => {
    res.json(await run(['health', req.params.id, '--json']));
}));

module.exports = router;
