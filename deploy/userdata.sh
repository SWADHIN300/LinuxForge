#!/bin/bash
# ============================================================
# LinuXForge — EC2 User Data Bootstrap Script
# Runs automatically on first boot as root.
# Region: ap-south-1 | Instance: t3.xlarge (4vCPU / 16GB RAM)
# Auto-terminates: April 25 18:30 UTC (April 26 00:00 IST)
# Estimated cost: ~$17-18 of your $30 budget
# ============================================================
set -euo pipefail
exec > /var/log/linuxforge-setup.log 2>&1

echo "========================================"
echo " LinuXForge Bootstrap — $(date -u)"
echo "========================================"

# ── 1. System update ─────────────────────────────────────────
apt-get update -y
apt-get upgrade -y
apt-get install -y \
  gcc make git curl wget unzip \
  nginx \
  iproute2 iptables \
  criu \
  build-essential \
  ca-certificates gnupg lsb-release \
  jq htop ncdu tree

# ── 2. Node.js 20 LTS ────────────────────────────────────────
curl -fsSL https://deb.nodesource.com/setup_20.x | bash -
apt-get install -y nodejs
npm install -g pm2

# Verify
node --version
npm --version
pm2 --version

# ── 3. Kernel tuning for max performance ─────────────────────
# Enable BBR TCP (better throughput)
echo "net.core.default_qdisc=fq"       >> /etc/sysctl.conf
echo "net.ipv4.tcp_congestion_control=bbr" >> /etc/sysctl.conf
# Increase open file limits (many containers = many file handles)
echo "fs.file-max = 2097152"           >> /etc/sysctl.conf
echo "net.core.somaxconn = 65535"      >> /etc/sysctl.conf
echo "net.ipv4.ip_forward = 1"         >> /etc/sysctl.conf
sysctl -p || true

cat >> /etc/security/limits.conf << 'EOF'
* soft nofile 1048576
* hard nofile 1048576
root soft nofile 1048576
root hard nofile 1048576
EOF

# ── 4. Clone the LinuXForge repository ───────────────────────
mkdir -p /opt/linuxforge
git clone --branch feat/improv --depth 1 \
  https://github.com/SWADHIN300/LinuxForge.git \
  /opt/linuxforge

cd /opt/linuxforge

# ── 5. Build the C engine binary ─────────────────────────────
make clean 2>/dev/null || true
make -j$(nproc) CFLAGS="-Wall -Wextra -O2 -Iinclude"
chmod +x mycontainer
echo "[OK] C binary built: $(./mycontainer 2>&1 | head -1 || true)"

# ── 6. Build Next.js frontend ────────────────────────────────
cd /opt/linuxforge/frontend
npm ci --prefer-offline
npm run build
echo "[OK] Next.js built"

# ── 7. Install backend dependencies ──────────────────────────
cd /opt/linuxforge/backend
npm ci --prefer-offline
echo "[OK] Backend deps installed"

cd /opt/linuxforge

# ── 8. PM2 ecosystem config ──────────────────────────────────
cat > /opt/linuxforge/deploy/ecosystem.config.js << 'EOF'
module.exports = {
  apps: [
    {
      name: 'linuxforge-frontend',
      cwd: '/opt/linuxforge/frontend',
      script: 'node_modules/.bin/next',
      args: 'start -p 3000',
      instances: 2,
      exec_mode: 'cluster',
      max_memory_restart: '3G',
      env: {
        NODE_ENV: 'production',
        PORT: 3000
      },
      error_file: '/var/log/linuxforge-frontend.err',
      out_file: '/var/log/linuxforge-frontend.out',
      merge_logs: true,
      restart_delay: 3000,
    },
    {
      name: 'linuxforge-bridge',
      cwd: '/opt/linuxforge',
      script: 'backend/src/server.js',
      instances: 1,
      exec_mode: 'fork',
      max_memory_restart: '1G',
      env: {
        NODE_ENV: 'production',
        PORT: 3001,
        MYCONTAINER_NO_SOCKET: '0'
      },
      error_file: '/var/log/linuxforge-bridge.err',
      out_file: '/var/log/linuxforge-bridge.out',
      merge_logs: true,
      restart_delay: 3000,
    }
  ]
};
EOF

# ── 9. Start PM2 apps ────────────────────────────────────────
pm2 start /opt/linuxforge/deploy/ecosystem.config.js
pm2 save
pm2 startup systemd -u root --hp /root | tail -1 | bash || true
echo "[OK] PM2 apps started"

# ── 10. Nginx reverse proxy ───────────────────────────────────
cat > /etc/nginx/sites-available/linuxforge << 'NGINXCONF'
upstream frontend {
    least_conn;
    server 127.0.0.1:3000;
    keepalive 64;
}

upstream api {
    least_conn;
    server 127.0.0.1:3001;
    keepalive 32;
}

server {
    listen 80 default_server;
    listen [::]:80 default_server;

    server_name _;

    client_max_body_size 100M;
    keepalive_timeout 65;

    # Gzip compression
    gzip on;
    gzip_vary on;
    gzip_proxied any;
    gzip_comp_level 6;
    gzip_types text/plain text/css text/xml text/javascript
               application/json application/javascript application/xml+rss
               application/atom+xml image/svg+xml;

    # Security headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;

    # API routes → Express bridge
    location /api/ {
        proxy_pass http://api/api/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_cache_bypass $http_upgrade;
        proxy_read_timeout 120s;
    }

    # SSE / streaming logs
    location /api/logs/ {
        proxy_pass http://api/api/logs/;
        proxy_http_version 1.1;
        proxy_set_header Connection '';
        proxy_buffering off;
        proxy_cache off;
        proxy_read_timeout 300s;
        chunked_transfer_encoding on;
    }

    # Static Next.js assets — cached at edge
    location /_next/static/ {
        proxy_pass http://frontend;
        proxy_cache_valid 200 365d;
        add_header Cache-Control "public, max-age=31536000, immutable";
    }

    # Everything else → Next.js frontend
    location / {
        proxy_pass http://frontend;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_cache_bypass $http_upgrade;
        proxy_read_timeout 60s;
    }
}
NGINXCONF

rm -f /etc/nginx/sites-enabled/default
ln -sf /etc/nginx/sites-available/linuxforge /etc/nginx/sites-enabled/

# Test and reload Nginx
nginx -t
systemctl enable nginx
systemctl restart nginx
echo "[OK] Nginx configured and running"

# ── 11. Network init for container engine ────────────────────
cd /opt/linuxforge
./mycontainer network init 2>/dev/null || true
echo "[OK] Container network initialized"

# ── 12. Auto-termination cron job ────────────────────────────
# April 25 18:30 UTC = April 26 00:00 IST
# Get this instance's ID for a clean self-terminate via AWS CLI
INSTANCE_ID=$(curl -s http://169.254.169.254/latest/meta-data/instance-id)
REGION=$(curl -s http://169.254.169.254/latest/meta-data/placement/region)

echo "Instance ID: $INSTANCE_ID  Region: $REGION"

# Install AWS CLI for self-terminate
apt-get install -y awscli 2>/dev/null || true

# Write auto-termination script
cat > /opt/linuxforge/deploy/autoterminate.sh << TERMINATE_SCRIPT
#!/bin/bash
# Auto-termination: April 26 00:00 IST (April 25 18:30 UTC)
INSTANCE_ID=\$(curl -s http://169.254.169.254/latest/meta-data/instance-id)
REGION=\$(curl -s http://169.254.169.254/latest/meta-data/placement/region)
echo "[\$(date -u)] LinuXForge auto-termination triggered" >> /var/log/linuxforge-terminate.log
# Graceful PM2 shutdown
pm2 stop all 2>/dev/null || true
# AWS self-terminate
aws ec2 terminate-instances --instance-ids \$INSTANCE_ID --region \$REGION 2>/dev/null || true
# Fallback: OS shutdown (triggers terminate due to instance flag)
sleep 10
/sbin/shutdown -h now
TERMINATE_SCRIPT

chmod +x /opt/linuxforge/deploy/autoterminate.sh

# Cron: April 25 18:30 UTC  (= April 26 00:00 IST)
(crontab -l 2>/dev/null; echo "30 18 25 4 * /opt/linuxforge/deploy/autoterminate.sh >> /var/log/linuxforge-terminate.log 2>&1") | crontab -

echo "[OK] Auto-termination cron set: April 25 18:30 UTC (April 26 00:00 IST)"

# ── 13. System info banner ────────────────────────────────────
PUBLIC_IP=$(curl -s http://169.254.169.254/latest/meta-data/public-ipv4 || echo "pending")

cat > /etc/motd << MOTD
============================================================
  LinuXForge Container Simulator
  URL    : http://$PUBLIC_IP
  API    : http://$PUBLIC_IP/api
  PM2    : pm2 status
  Logs   : pm2 logs
  Nginx  : systemctl status nginx
  C-Engine: /opt/linuxforge/mycontainer
  AUTO-DELETES: April 25 18:30 UTC (April 26 00:00 IST)
============================================================
MOTD

echo "========================================"
echo " DONE! LinuXForge is live at:"
echo " http://$PUBLIC_IP"
echo " Auto-terminates: April 25 18:30 UTC"
echo "========================================"
