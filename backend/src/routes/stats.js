const express = require('express');

const { asyncHandler, run } = require('../lib/http');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await run(['stats', '--all', '--json']));
}));

router.get('/:id', asyncHandler(async (req, res) => {
    const argv = ['stats', req.params.id];

    if (req.query.history) {
        argv.push(`--history=${Number(req.query.history) || 60}`);
    }

    argv.push('--json');
    res.json(await run(argv));
}));

module.exports = router;
