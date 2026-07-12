#!/bin/bash

echo "=== SSCK Sync ==="

cd "$(dirname "$0")"

echo ""
echo "[1/5] Salvo temporaneamente le modifiche locali..."
git stash

echo ""
echo "[2/5] Scarico gli ultimi aggiornamenti..."
git pull --rebase

if [ $? -ne 0 ]; then
    echo ""
    echo "Errore durante il pull."
    exit 1
fi

echo ""
echo "[3/5] Ripristino le modifiche locali..."
git stash pop

echo ""
echo "[4/5] Creo il commit..."
git add .

read -p "Messaggio del commit: " MSG

git commit -m "$MSG"

echo ""
echo "[5/5] Invio le modifiche..."
git push

echo ""
echo "✅ Sincronizzazione completata!"
