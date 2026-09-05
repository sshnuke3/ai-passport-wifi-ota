#!/usr/bin/env bash
# push-to-github.sh —— 把 feature/wifi-ota 分支推到你的 GitHub 远端。
#
# 用法（按顺序）：
#   1) 在 GitHub 网页上创建一个空仓库（不要勾 README / .gitignore / License），
#      名字建议 `ai-passport-wifi-ota` 或类似。
#   2) 把仓库 URL 写到下面 REMOTE_URL，或者用环境变量 GITHUB_URL 覆盖。
#   3) bash scripts/push-to-github.sh
#
# 脚本会：
#   - 用 SSH/HTTPS 任一可达方式加 remote
#   - 推 feature/wifi-ota 分支
#   - 打印一份 README 链接清单供分享用

set -euo pipefail

BRANCH="feature/wifi-ota"
REMOTE_NAME="origin-fork"
REMOTE_URL="${GITHUB_URL:-git@github.com:YOUR_USERNAME/ai-passport-wifi-ota.git}"

if [[ "$REMOTE_URL" == *"YOUR_USERNAME"* ]]; then
  echo "ERROR: 先在 GitHub 建空仓，然后把 URL 写到 REMOTE_URL 或设环境变量 GITHUB_URL" >&2
  exit 1
fi

cd "$(dirname "$0")/.."   # 回到仓库根

echo "[1/3] 检查 git 状态..."
git status --short
if ! git diff --quiet || ! git diff --cached --quiet || [[ -n "$(git status --porcelain)" ]]; then
  echo ""
  echo "工作区有未提交的改动："
  git status --short
  echo ""
  read -rp "是否先 git add + commit？[y/N] " ans
  case "$ans" in
    y|Y) git add -A
         git commit -m "feat(ota): Wi-Fi OTA loader — partitions + AP + http upload + menu entry"
         ;;
    *) echo "中止：请先手动 commit 再跑"; exit 2 ;;
  esac
fi

echo "[2/3] 设置 remote..."
if git remote get-url "$REMOTE_NAME" >/dev/null 2>&1; then
  echo "  remote $REMOTE_NAME 已存在：$(git remote get-url "$REMOTE_NAME")"
else
  git remote add "$REMOTE_NAME" "$REMOTE_URL"
  echo "  添加 $REMOTE_NAME -> $REMOTE_URL"
fi

echo "[3/3] 推送 $BRANCH..."
git push -u "$REMOTE_NAME" "$BRANCH"

cat <<EOF

✅ 推送完成。

分享链接（直接给朋友刷机用）：
  仓库：  $REMOTE_URL
  分支：  $BRANCH
  README：$REMOTE_URL/blob/$BRANCH/README.md
  中文：  $REMOTE_URL/blob/$BRANCH/README.zh_CN.md

建议给朋友的快速指南（直接复制）：
  1. 克隆：git clone -b $BRANCH $REMOTE_URL ai-passport-ota
  2. 构建：cd ai-passport-ota && idf.py build
  3. 合并：cd build && idf merge-bin -o FoloToy-AI-Passport-full.bin
  4. 验证：cd .. && python tools/verify_firmware.py build
  5. USB 烧：用官方网页刷机工具烧第 4 步得到的 full.bin（0x0 偏移）
  6. 后续玩法用菜单里 Wireless Update 推 OTA
EOF