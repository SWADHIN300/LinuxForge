# LinuXForge — AWS Console Deployment Guide
# BUDGET: ~$30 | Instance: t3.xlarge (4 vCPU / 16 GB) | ap-south-1 | Auto-deletes April 26 00:00 IST

---

## What You Need
- AWS account with $100 credit ✅
- A web browser ✅
- 10 minutes ✅

---

## STEP 1 — Open AWS Console

Go to: **https://console.aws.amazon.com**

Log in → top-right corner → change **Region to `Asia Pacific (Mumbai) ap-south-1`**

---

## STEP 2 — Create a Security Group

> EC2 → Security Groups → **Create security group**

**Name:** `linuxforge-sg`  
**Description:** `LinuXForge ports`  
**VPC:** (leave default)

**Inbound rules** — click "Add rule" for each:

| Type | Protocol | Port | Source |
|---|---|---|---|
| SSH | TCP | 22 | Anywhere IPv4 (`0.0.0.0/0`) |
| HTTP | TCP | 80 | Anywhere IPv4 (`0.0.0.0/0`) |
| HTTPS | TCP | 443 | Anywhere IPv4 (`0.0.0.0/0`) |
| Custom TCP | TCP | 3001 | Anywhere IPv4 (`0.0.0.0/0`) |

Click **Create security group**

---

## STEP 3 — Create a Key Pair (for SSH access)

> EC2 → Key Pairs → **Create key pair**

**Name:** `linuxforge-key`  
**Type:** RSA  
**Format:** `.pem` (Mac/Linux) or `.ppk` (Windows PuTTY)

Click **Create key pair** — it downloads the file automatically.  
**Save it somewhere safe** — you need it to SSH in.

---

## STEP 4 — Launch the EC2 Instance

> EC2 → Instances → **Launch Instances**

Fill in these fields:

### Name
```
LinuXForge-Max
```

### Application and OS Image
- Click **Ubuntu**
- Select: **Ubuntu Server 22.04 LTS (HVM), SSD Volume Type**
- Architecture: **64-bit (x86)**

### Instance Type
Search and select: **`t3.xlarge`**
> 4 vCPU · 16 GB RAM · Up to 5 Gbps network · ~$0.1664/hr

### Key Pair
Select: **`linuxforge-key`** (the one you just created)

### Network Settings
- Click **Edit**
- VPC: (default)
- Subnet: (any)
- Auto-assign public IP: **Enable**
- Firewall: **Select existing security group** → pick **`linuxforge-sg`**

### Configure Storage
- **100 GiB**
- Volume type: **gp3**
- IOPS: **3000**
- Throughput: **125 MB/s**
- ✅ Check **"Delete on termination"** ← IMPORTANT

### Advanced Details → User Data

Scroll down to **Advanced details** → expand it → find **"User data"** text box.

**Copy the ENTIRE contents of `deploy/userdata.sh`** and paste it here.

> The script automatically:
> - Installs gcc, Node.js 20, Nginx, PM2
> - Clones your GitHub repo (`feat/improv` branch)
> - Builds the C binary with `make`
> - Builds Next.js frontend
> - Starts everything with PM2
> - Configures Nginx as reverse proxy
> - Sets up auto-termination: **April 26 00:00 IST**

### Termination Protection
In Advanced details → **"Termination protection"** → **Disable**
(We WANT it to auto-terminate)

### Shutdown behavior
In Advanced details → **"Shutdown behavior"** → **Terminate**
(This ensures the instance is DELETED, not just stopped, when shutdown runs)

---

## STEP 5 — Launch!

Click the orange **"Launch instance"** button.

AWS will show you the instance ID. Click it to go to the instance page.

---

## STEP 6 — Get Your Public IP

Wait 2-3 minutes for the instance to show **"Running"** status.

Click on the instance → copy the **"Public IPv4 address"**

Your app will be live at:
```
http://<PUBLIC-IP>
```

> **Setup takes ~8-10 minutes** after launch (Node.js install, npm build, etc.)
> You can watch progress via SSH (see below).

---

## STEP 7 — Set Up EventBridge Auto-Termination (Backup Kill Switch)

> Go to: **EventBridge → Rules → Create rule**

**Name:** `linuxforge-autodelete`  
**Description:** `Delete LinuXForge instance on April 25 18:30 UTC`  
**Event bus:** default  
**Rule type:** Schedule  

Click **Next**

**Schedule pattern:** Cron expression
```
30 18 25 4 ? 2026
```
> This means: April 25 at 18:30 UTC = April 26 00:00 IST

Click **Next**

**Target type:** AWS service  
**Target:** EC2 → TerminateInstances  
**Instance ID:** (paste your instance ID from Step 6)

> If TerminateInstances isn't in the list, choose **"EC2" → CreateTags** won't work.
> Use this alternative: Target = **SNS topic** → Lambda → but that's complex.
> Instead, the simpler backup:

**Alternative backup target (simpler):**
- Target type: **AWS service**
- Target: **AWS Lambda** → create a tiny function

Or just rely on the cron job inside the server (Layer 2) — it will self-terminate reliably.

---

## STEP 8 — Monitor Your App

### View setup logs (SSH in)
```bash
# On Mac/Linux:
chmod 400 linuxforge-key.pem
ssh -i linuxforge-key.pem ubuntu@<YOUR-PUBLIC-IP>

# Then check setup progress:
tail -f /var/log/linuxforge-setup.log

# Check if everything is running:
pm2 status

# View app logs:
pm2 logs

# Check Nginx:
systemctl status nginx
```

### URLs
| Service | URL |
|---|---|
| **Frontend Dashboard** | `http://<IP>/` |
| **API Health** | `http://<IP>/api/containers` |
| **Container Images** | `http://<IP>/api/images` |

---

## What Auto-Deletes on April 25 18:30 UTC (April 26 00:00 IST)

| Resource | Auto-deleted? |
|---|---|
| EC2 instance | ✅ Yes (cron + shutdown behavior = terminate) |
| EBS 100 GB disk | ✅ Yes ("Delete on termination" checked) |
| EventBridge rule | ✅ Yes (one-time rule fires once and is done) |
| Security Group | ❌ No (free, but manually delete if you want) |
| Key Pair | ❌ No (free, manually delete if you want) |

**After termination: $0.00 ongoing cost.** ✅

---

## Cost Estimate (~$30 budget)

| Resource | Rate | Duration | Cost |
|---|---|---|
|---|
| t3.xlarge (ap-south-1) | ~$0.1664/hr | ~96 hrs | ~$16.00 |
| EBS gp3 100 GB | ~$0.096/GB-mo | 4 days | ~$1.30 |
| Data transfer out | $0.09/GB | ~5 GB | ~$0.45 |
| **TOTAL** | | | **~$17.75** |

**Remaining from $30 budget: ~$12 buffer ✅**

> Your AWS credits are safe — this runs well within the $30 target.

---

## Troubleshooting

**Site not loading after 15 mins?**
```bash
ssh -i linuxforge-key.pem ubuntu@<IP>
cat /var/log/linuxforge-setup.log | tail -50
pm2 status
```

**PM2 apps crashed?**
```bash
pm2 restart all
pm2 logs --lines 100
```

**Nginx not working?**
```bash
nginx -t
systemctl restart nginx
```

**Check C binary works:**
```bash
cd /opt/linuxforge
./mycontainer image ls --json
```
