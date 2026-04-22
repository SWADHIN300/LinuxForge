function normalizeApiOrigin(value) {
  if (!value) {
    return 'http://localhost:3001';
  }

  return value.replace(/\/+$/, '').replace(/\/api$/, '');
}

const apiOrigin = normalizeApiOrigin(process.env.API_BASE_URL);

/** @type {import('next').NextConfig} */
const nextConfig = {
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
