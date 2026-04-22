import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

function normalizeApiOrigin(value) {
  if (!value) {
    return 'http://localhost:3001';
  }

  return value.replace(/\/+$/, '').replace(/\/api$/, '');
}

const apiOrigin = normalizeApiOrigin(process.env.API_BASE_URL);

/** @type {import('next').NextConfig} */
const nextConfig = {
  turbopack: {
    root: __dirname,
  },
  async rewrites() {
    return [
      {
        source: '/api/:path*',
        destination: `${apiOrigin}/api/:path*`,
      },
    ];
  },
};

export default nextConfig;
