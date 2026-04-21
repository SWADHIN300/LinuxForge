const express = require('express');

const { asyncHandler, run } = require('../lib/http');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await run(['dns', 'ls', '--json']));
}));

router.post('/update', asyncHandler(async (_req, res) => {
    res.json(await run(['dns', 'update', '--json']));
}));

module.exports = router;
