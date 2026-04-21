const express = require('express');

const { bridge } = require('../lib/simulator');
const { asyncHandler, run } = require('../lib/http');

const router = express.Router();

router.get('/', asyncHandler(async (_req, res) => {
    res.json(await bridge.commit.containers());
}));

router.get('/:id', asyncHandler(async (req, res) => {
    const argv = ['logs', req.params.id];

    if (req.query.tail) {
        argv.push(`--tail=${Number(req.query.tail) || 10}`);
    }

    argv.push('--json');
    res.json(await run(argv));
}));

router.delete('/:id', asyncHandler(async (req, res) => {
    res.json(await run(['logs', req.params.id, '--clear', '--json']));
}));

module.exports = router;
