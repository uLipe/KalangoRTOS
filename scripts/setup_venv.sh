#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="${ROOT}/.venv"

python3 -m venv "${VENV}"
"${VENV}/bin/pip" install --upgrade pip
"${VENV}/bin/pip" install -r "${ROOT}/scripts/requirements.txt"

echo "Virtualenv ready at ${VENV}"
echo "Activate with: source ${VENV}/bin/activate"
