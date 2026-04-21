const express = require('express');

const { asyncHandler, run } = require('../lib/http');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await run(['security', 'ls', '--json']));
}));

router.get('/:profile', asyncHandler(async (req, res) => {
    res.json(await run(['security', 'inspect', req.params.profile, '--json']));
}));

module.exports = router;
