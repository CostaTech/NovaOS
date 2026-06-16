#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

message="$*"
if [ -z "$message" ]; then
    message="NovaOS update $(date '+%Y-%m-%d %H:%M:%S')"
fi

if [ ! -d .git ]; then
    echo "[NovaOS] Git repo not found. Initializing..."
    git init
fi

git add .

if git diff --cached --quiet; then
    echo "[NovaOS] Nothing to save. Working tree has no staged changes."
    exit 0
fi

git commit -m "$message"

echo "[NovaOS] Saved commit: $message"

if git remote get-url origin >/dev/null 2>&1; then
    echo "[NovaOS] Remote origin found. Pushing..."
    git push origin HEAD
else
    echo "[NovaOS] No remote origin configured. Local commit saved only."
    echo "[NovaOS] Later you can add GitHub with: git remote add origin <URL>"
fi
