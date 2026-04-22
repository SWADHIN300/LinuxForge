const express = require('express');
const path = require('path');
const { ensureDir } = require('./lib/data');

const logsRoute = require('./routes/logs');
const statsRoute = require('./routes/stats');
const healthRoute = require('./routes/health');
const volumesRoute = require('./routes/volumes');
const stacksRoute = require('./routes/stacks');
const securityRoute = require('./routes/security');
const dnsRoute = require('./routes/dns');
const exportRoute = require('./routes/export');
const checkpointsRoute = require('./routes/checkpoints');
const registryRoute = require('./routes/registry');
const containersRoute = require('./routes/containers');

const app = express();
const exportsDir = path.join(__dirname, '..', 'exports');

app.use(express.json({ limit: '2mb' }));
app.use(express.urlencoded({ extended: true }));

ensureDir(exportsDir);
app.use('/exports', express.static(exportsDir));

app.use('/api/logs', logsRoute);
app.use('/api/containers', containersRoute);
app.use('/api/stats', statsRoute);
app.use('/api/health', healthRoute);
app.use('/api/volumes', volumesRoute);
app.use('/api/stacks', stacksRoute);
app.use('/api/security', securityRoute);
app.use('/api/dns', dnsRoute);
app.use('/api/export', exportRoute);
app.use('/api/checkpoints', checkpointsRoute);
app.use('/api/registry', registryRoute);

app.get('/api/healthz', (_req, res) => {
    res.json({ ok: true });
});

app.use((_req, res) => {
    res.status(404).json({ ok: false, error: 'Not found' });
});

app.use((err, _req, res, _next) => {
    const status = Number(err && err.status) || 500;
    const message = err && err.message ? err.message : 'Unexpected server error';

    if (status >= 500) {
        console.error(err);
    }

    res.status(status).json({
        ok: false,
        error: message,
        details: err && err.details ? err.details : undefined,
    });
});

module.exports = app;
