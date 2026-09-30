#!/bin/bash
set -e # Avbryter skriptet direkt om något kommando misslyckas

echo "Kopierar pintos till hemkatalogen..."
cp -r ~/os_labs/Lab3/pintos ~/

echo "Uppdaterar PATH..."
export PATH=/chalmers/sw/unsup64/phc/b/pkg/bochs-2.6.6/bin:$HOME/pintos/src/utils:$PATH

echo "Laddar om .bashrc..."
source $HOME/.bashrc

echo "Sätter exekverbara rättigheter..."
cd ~/os_labs/Lab3
chmod +x pintos/src/utils/pintos*
chmod +x pintos/src/utils/backtrace

echo "Kör make check..."
cd ~/os_labs/Lab3/pintos/src/threads/build
make check

echo "Klar!"