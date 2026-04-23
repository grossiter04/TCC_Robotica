#!/bin/bash
# ============================================================
#  BuzzLine — Script de inicialização
#  Uso: ./start.sh
#  Para encerrar tudo: Ctrl+C
# ============================================================

# --- CAMINHOS ---
VENV_PATH="$HOME/tcc-venv"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_PATH="$SCRIPT_DIR/server.py"
FOLLOWER_PATH="$SCRIPT_DIR/build/line_follower"

# --- ARGUMENTO: --display usa monitor físico, sem argumento usa Xvfb ---
USE_PHYSICAL_DISPLAY=false
for arg in "$@"; do
    if [ "$arg" == "--display" ]; then
        USE_PHYSICAL_DISPLAY=true
    fi
done

# --- CORES PARA O LOG ---
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # sem cor

log()  { echo -e "${CYAN}[BuzzLine]${NC} $1"; }
ok()   { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[AVISO]${NC} $1"; }
err()  { echo -e "${RED}[ERRO]${NC} $1"; }

echo ""
echo -e "${CYAN}╔══════════════════════════════════╗${NC}"
echo -e "${CYAN}║      BuzzLine — Iniciando        ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════╝${NC}"
echo ""

# --- VERIFICA SE O EXECUTÁVEL FOI COMPILADO ---
if [ ! -f "$FOLLOWER_PATH" ]; then
    err "Executável não encontrado em: $FOLLOWER_PATH"
    warn "Compile primeiro com:"
    warn "  cd $SCRIPT_DIR && mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

# --- VERIFICA SE O VENV EXISTE ---
if [ ! -d "$VENV_PATH" ]; then
    err "Ambiente virtual não encontrado em: $VENV_PATH"
    warn "Crie com: python3 -m venv $VENV_PATH && source $VENV_PATH/bin/activate && pip install flask"
    exit 1
fi

# --- ENCERRAMENTO LIMPO COM CTRL+C ---
cleanup() {
    echo ""
    log "Encerrando todos os processos..."
    kill $PID_XVFB     2>/dev/null
    kill $PID_SERVER   2>/dev/null
    kill $PID_FOLLOWER 2>/dev/null
    wait 2>/dev/null
    ok "Tudo encerrado. Até mais!"
    exit 0
}
trap cleanup SIGINT SIGTERM

# --- 1. CONFIGURA O DISPLAY ---
PID_XVFB=""
if [ "$USE_PHYSICAL_DISPLAY" = true ]; then
    log "Modo monitor físico ativado."
    export DISPLAY=:0
    ok "Usando DISPLAY=:0"
else
    # Instala Xvfb se necessário
    if ! command -v Xvfb &> /dev/null; then
        warn "Xvfb não encontrado. Instalando..."
        sudo apt-get install -y xvfb > /dev/null 2>&1
        ok "Xvfb instalado."
    fi

    log "Iniciando display virtual (Xvfb)..."
    DISPLAY_NUM=:99
    Xvfb $DISPLAY_NUM -screen 0 640x480x24 &
    PID_XVFB=$!
    sleep 1

    if kill -0 $PID_XVFB 2>/dev/null; then
        ok "Display virtual ativo em $DISPLAY_NUM (PID: $PID_XVFB)"
    else
        err "Falha ao iniciar o Xvfb."
        exit 1
    fi

    export DISPLAY=$DISPLAY_NUM
fi

# --- 2. INICIA O SERVIDOR WEB ---
log "Iniciando servidor web Flask..."
source "$VENV_PATH/bin/activate"
python3 "$SERVER_PATH" &
PID_SERVER=$!
sleep 2

if kill -0 $PID_SERVER 2>/dev/null; then
    # Descobre o IP da Raspberry para exibir o link
    IP=$(hostname -I | awk '{print $1}')
    ok "Servidor rodando em http://$IP:5000 (PID: $PID_SERVER)"
else
    err "Falha ao iniciar o servidor Flask."
    kill $PID_XVFB 2>/dev/null
    exit 1
fi

# --- 3. INICIA O LINE FOLLOWER ---
log "Iniciando line_follower..."
"$FOLLOWER_PATH" &
PID_FOLLOWER=$!
sleep 1

if kill -0 $PID_FOLLOWER 2>/dev/null; then
    ok "line_follower rodando (PID: $PID_FOLLOWER)"
else
    err "Falha ao iniciar o line_follower."
    kill $PID_XVFB   2>/dev/null
    kill $PID_SERVER 2>/dev/null
    exit 1
fi

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Tudo rodando! Acesse a interface em:        ║${NC}"
echo -e "${GREEN}║  http://$(hostname -I | awk '{print $1}'):5000                   ║${NC}"
echo -e "${GREEN}║                                              ║${NC}"
echo -e "${GREEN}║  Pressione Ctrl+C para encerrar tudo.        ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════╝${NC}"
echo ""

# --- AGUARDA ATÉ CTRL+C OU UM DOS PROCESSOS MORRER ---
while true; do
    if ! kill -0 $PID_FOLLOWER 2>/dev/null; then
        warn "line_follower encerrou inesperadamente."
        cleanup
    fi
    if ! kill -0 $PID_SERVER 2>/dev/null; then
        warn "Servidor Flask encerrou inesperadamente."
        cleanup
    fi
    sleep 2
done