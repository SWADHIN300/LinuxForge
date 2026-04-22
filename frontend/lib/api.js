const RAW_BASE =
  (typeof process !== 'undefined' && process.env.NEXT_PUBLIC_API_BASE_URL) ||
  '/api';

const BASE = RAW_BASE.replace(/\/+$/, '');

function apiUrl(path) {
  return `${BASE}${path}`;
}

async function request(url, options = {}) {
  const headers = { ...options.headers };
  const isFormData = typeof FormData !== 'undefined' && options.body instanceof FormData;

  if (!isFormData && !headers['Content-Type']) {
    headers['Content-Type'] = 'application/json';
  }

  const res = await fetch(apiUrl(url), {
    headers,
    ...options,
  });
  if (!res.ok) {
    const body = await res.json().catch(() => ({ error: res.statusText }));
    throw new Error(body.error || `HTTP ${res.status}`);
  }
  return res.json();
}

const api = {
  get: (url) => request(url),
  post: (url, body) => request(url, { method: 'POST', body: JSON.stringify(body) }),
  postForm: (url, body) => request(url, { method: 'POST', body }),
  delete: (url) => request(url, { method: 'DELETE' }),
};

/* ── containers (reuses commit ls) ── */
export const containersApi = {
  list: () => api.get('/containers'),
  run: (payload) => api.post('/containers/run', payload),
};

/* ── logs ── */
export const logsApi = {
  get: (id, tail = 100) => api.get(`/logs/${id}?tail=${tail}`),
  clear: (id) => api.delete(`/logs/${id}`),
  stream: (id) => {
    if (typeof window === 'undefined') return null;
    return new EventSource(apiUrl(`/logs/${id}/stream`));
  },
};

/* ── stats ── */
export const statsApi = {
  current: (id) => api.get(`/stats/${id}`),
  history: (id, mins = 60) => api.get(`/stats/${id}?history=${mins}`),
  all: () => api.get('/stats'),
  stream: (id) => {
    if (typeof window === 'undefined') return null;
    return new EventSource(apiUrl(`/stats/${id}/stream`));
  },
};

/* ── health ── */
export const healthApi = {
  get: (id) => api.get(`/health/${id}`),
  all: () => api.get('/health'),
  check: (id) => api.post(`/health/${id}/run`, {}),
};

/* ── volumes ── */
export const volumesApi = {
  list: () => api.get('/volumes'),
  getForContainer: (id) => api.get(`/volumes/${id}`),
};

/* ── stacks ── */
export const stacksApi = {
  list: () => api.get('/stacks'),
  get: (name) => api.get(`/stacks/${name}`),
  up: (file) => api.post('/stacks/up', { file }),
  down: (name) => api.post('/stacks/down', { name }),
};

/* ── security ── */
export const securityApi = {
  list: () => api.get('/security'),
  inspect: (profile) => api.get(`/security/${profile}`),
};

/* ── dns ── */
export const dnsApi = {
  list: () => api.get('/dns'),
  update: () => api.post('/dns/update', {}),
};

/* ── checkpoints ── */
export const checkpointsApi = {
  list: () => api.get('/checkpoints'),
  create: (id, checkpointDir) => api.post(`/checkpoints/${id}`, { checkpointDir }),
  restore: (checkpointDir, name) => api.post('/checkpoints/restore', { checkpointDir, name }),
};

/* ── registry ── */
export const registryApi = {
  list: () => api.get('/registry'),
  build: (payload) => api.post('/registry/build', payload),
  upload: (archive, image) => {
    const form = new FormData();
    form.append('archive', archive);
    form.append('image', image);
    return api.postForm('/registry/import-upload', form);
  },
};

/* ── network ── */
export const networkApi = {
  topology: () => api.get('/export').catch(() => ({ nodes: [], edges: [] })),
};

/* ── commit ── */
export const commitApi = {
  list: () => api.get('/logs'),
};
