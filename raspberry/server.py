"""
BuzzLine - Servidor de Controle PD em Tempo Real
Hospeda uma interface web para ajustar Kp, Kd e velocidade_base
enquanto o robô está em movimento.

Uso:
    pip install flask
    python3 server.py

Acesse em: http://<IP-da-Raspberry>:5000
"""

import os
import json
import time
from flask import Flask, request, jsonify, render_template_string

app = Flask(__name__)

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "buzzline_config.json")

DEFAULT_CONFIG = {
    "kp": 0.5,
    "kd": 0.0,
    "velocidade_base": 130,
    "vel_min": 100,
    "vel_max": 255
}

# --- HTML da Interface ---
HTML = """
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>BuzzLine — Controle PD</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow:wght@300;400;600;700&display=swap" rel="stylesheet">
<style>
  :root {
    --bg:        #0a0c0f;
    --surface:   #111419;
    --border:    #1e2530;
    --accent:    #00e5ff;
    --accent2:   #ff3d71;
    --warn:      #ffaa00;
    --text:      #c8d6e5;
    --muted:     #4a5568;
    --mono:      'Share Tech Mono', monospace;
    --sans:      'Barlow', sans-serif;
  }

  * { margin: 0; padding: 0; box-sizing: border-box; }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: var(--sans);
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 24px 16px 48px;
    background-image:
      radial-gradient(ellipse 60% 40% at 80% 0%, rgba(0,229,255,0.04) 0%, transparent 60%),
      radial-gradient(ellipse 40% 30% at 10% 90%, rgba(255,61,113,0.03) 0%, transparent 50%),
      repeating-linear-gradient(0deg, transparent, transparent 39px, rgba(255,255,255,0.015) 40px),
      repeating-linear-gradient(90deg, transparent, transparent 39px, rgba(255,255,255,0.015) 40px);
  }

  header {
    width: 100%;
    max-width: 560px;
    display: flex;
    align-items: flex-end;
    gap: 16px;
    margin-bottom: 32px;
    padding-bottom: 20px;
    border-bottom: 1px solid var(--border);
  }

  .logo {
    font-family: var(--mono);
    font-size: 1.5rem;
    color: var(--accent);
    letter-spacing: 0.08em;
    line-height: 1;
  }

  .logo span {
    color: var(--accent2);
  }

  .subtitle {
    font-size: 0.72rem;
    color: var(--muted);
    letter-spacing: 0.12em;
    text-transform: uppercase;
    font-weight: 600;
    margin-bottom: 2px;
  }

  .status-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--muted);
    margin-left: auto;
    margin-bottom: 4px;
    box-shadow: 0 0 0 2px var(--bg);
    transition: background 0.3s, box-shadow 0.3s;
  }
  .status-dot.online { background: #00e676; box-shadow: 0 0 8px #00e676; }
  .status-dot.sending { background: var(--warn); box-shadow: 0 0 8px var(--warn); }

  .card {
    width: 100%;
    max-width: 560px;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 28px;
    margin-bottom: 16px;
  }

  .card-title {
    font-family: var(--mono);
    font-size: 0.7rem;
    color: var(--muted);
    letter-spacing: 0.2em;
    text-transform: uppercase;
    margin-bottom: 24px;
    display: flex;
    align-items: center;
    gap: 10px;
  }

  .card-title::after {
    content: '';
    flex: 1;
    height: 1px;
    background: var(--border);
  }

  .param-row {
    margin-bottom: 28px;
  }

  .param-row:last-child { margin-bottom: 0; }

  .param-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 10px;
  }

  .param-label {
    font-family: var(--mono);
    font-size: 0.9rem;
    color: var(--text);
    letter-spacing: 0.04em;
  }

  .param-label em {
    font-style: normal;
    font-size: 0.72rem;
    color: var(--muted);
    margin-left: 8px;
  }

  .param-value {
    font-family: var(--mono);
    font-size: 1.2rem;
    color: var(--accent);
    min-width: 60px;
    text-align: right;
  }

  .param-value.red { color: var(--accent2); }
  .param-value.warn { color: var(--warn); }

  .slider-wrap {
    position: relative;
    display: flex;
    align-items: center;
    gap: 12px;
  }

  input[type=range] {
    -webkit-appearance: none;
    flex: 1;
    height: 3px;
    background: var(--border);
    border-radius: 2px;
    outline: none;
    cursor: pointer;
  }

  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 18px;
    height: 18px;
    border-radius: 50%;
    background: var(--accent);
    border: 2px solid var(--bg);
    box-shadow: 0 0 10px rgba(0,229,255,0.4);
    cursor: pointer;
    transition: transform 0.15s, box-shadow 0.15s;
  }

  input[type=range]:active::-webkit-slider-thumb {
    transform: scale(1.25);
    box-shadow: 0 0 18px rgba(0,229,255,0.7);
  }

  input[type=range].red::-webkit-slider-thumb {
    background: var(--accent2);
    box-shadow: 0 0 10px rgba(255,61,113,0.4);
  }

  input[type=range].warn::-webkit-slider-thumb {
    background: var(--warn);
    box-shadow: 0 0 10px rgba(255,170,0,0.4);
  }

  input[type=number] {
    width: 72px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 4px;
    color: var(--accent);
    font-family: var(--mono);
    font-size: 0.85rem;
    padding: 5px 8px;
    text-align: center;
    outline: none;
    transition: border-color 0.2s;
  }

  input[type=number]:focus { border-color: var(--accent); }
  input[type=number].red { color: var(--accent2); }
  input[type=number].red:focus { border-color: var(--accent2); }
  input[type=number].warn { color: var(--warn); }
  input[type=number].warn:focus { border-color: var(--warn); }

  .btn-apply {
    width: 100%;
    max-width: 560px;
    height: 52px;
    background: transparent;
    border: 1px solid var(--accent);
    border-radius: 6px;
    color: var(--accent);
    font-family: var(--mono);
    font-size: 0.95rem;
    letter-spacing: 0.15em;
    text-transform: uppercase;
    cursor: pointer;
    transition: background 0.2s, color 0.2s, box-shadow 0.2s;
    margin-bottom: 12px;
    position: relative;
    overflow: hidden;
  }

  .btn-apply:hover {
    background: rgba(0,229,255,0.08);
    box-shadow: 0 0 24px rgba(0,229,255,0.15);
  }

  .btn-apply:active {
    background: rgba(0,229,255,0.18);
  }

  .btn-apply.success {
    border-color: #00e676;
    color: #00e676;
    box-shadow: 0 0 24px rgba(0,230,118,0.2);
  }

  .btn-apply.error {
    border-color: var(--accent2);
    color: var(--accent2);
  }

  .log-box {
    width: 100%;
    max-width: 560px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 14px 16px;
    font-family: var(--mono);
    font-size: 0.72rem;
    color: var(--muted);
    line-height: 1.9;
    max-height: 130px;
    overflow-y: auto;
  }

  .log-box .entry { border-bottom: 1px solid rgba(255,255,255,0.04); padding-bottom: 2px; }
  .log-box .ts { color: rgba(255,255,255,0.2); margin-right: 8px; }
  .log-box .ok { color: #00e676; }
  .log-box .err { color: var(--accent2); }
  .log-box .info { color: var(--accent); }

  .hint {
    font-size: 0.7rem;
    color: var(--muted);
    text-align: center;
    margin-top: 6px;
    font-family: var(--mono);
    letter-spacing: 0.05em;
  }

  ::-webkit-scrollbar { width: 4px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb { background: var(--border); border-radius: 2px; }
</style>
</head>
<body>

<header>
  <div>
    <div class="subtitle">Line Follower PD</div>
    <div class="logo">Buzz<span>Line</span></div>
  </div>
  <div class="status-dot" id="dot" title="Status"></div>
</header>

<div class="card">
  <div class="card-title">Ganhos do Controlador</div>

  <div class="param-row">
    <div class="param-header">
      <span class="param-label">Kp <em>proporcional</em></span>
      <span class="param-value" id="kp-display">0.50</span>
    </div>
    <div class="slider-wrap">
      <input type="range" id="kp-slider" min="0" max="5" step="0.05" value="0.5">
      <input type="number" id="kp-input" class="" min="0" max="5" step="0.05" value="0.50">
    </div>
  </div>

  <div class="param-row">
    <div class="param-header">
      <span class="param-label">Kd <em>derivativo</em></span>
      <span class="param-value red" id="kd-display">0.00</span>
    </div>
    <div class="slider-wrap">
      <input type="range" id="kd-slider" class="red" min="0" max="10" step="0.1" value="0.0">
      <input type="number" id="kd-input" class="red" min="0" max="10" step="0.1" value="0.00">
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">Velocidade</div>

  <div class="param-row">
    <div class="param-header">
      <span class="param-label">Base <em>linha reta</em></span>
      <span class="param-value warn" id="vbase-display">130</span>
    </div>
    <div class="slider-wrap">
      <input type="range" id="vbase-slider" class="warn" min="80" max="255" step="1" value="130">
      <input type="number" id="vbase-input" class="warn" min="80" max="255" step="1" value="130">
    </div>
  </div>

  <div class="param-row">
    <div class="param-header">
      <span class="param-label">Vel. mínima</span>
      <span class="param-value warn" id="vmin-display">100</span>
    </div>
    <div class="slider-wrap">
      <input type="range" id="vmin-slider" class="warn" min="0" max="200" step="1" value="100">
      <input type="number" id="vmin-input" class="warn" min="0" max="200" step="1" value="100">
    </div>
  </div>
</div>

<button class="btn-apply" id="btn" onclick="aplicar()">▶ APLICAR</button>
<div class="hint" id="hint">Os valores são aplicados ao robô em tempo real, sem parar.</div>

<div class="log-box" id="log"></div>

<script>
  const params = [
    { key: 'kp',             slider: 'kp-slider',    input: 'kp-input',    display: 'kp-display',    decimals: 2 },
    { key: 'kd',             slider: 'kd-slider',    input: 'kd-input',    display: 'kd-display',    decimals: 2 },
    { key: 'velocidade_base',slider: 'vbase-slider', input: 'vbase-input', display: 'vbase-display', decimals: 0 },
    { key: 'vel_min',        slider: 'vmin-slider',  input: 'vmin-input',  display: 'vmin-display',  decimals: 0 },
  ];

  // Sincroniza slider <-> input <-> display
  params.forEach(p => {
    const slider  = document.getElementById(p.slider);
    const input   = document.getElementById(p.input);
    const display = document.getElementById(p.display);

    slider.addEventListener('input', () => {
      const v = parseFloat(slider.value).toFixed(p.decimals);
      input.value   = v;
      display.textContent = v;
    });

    input.addEventListener('input', () => {
      const v = parseFloat(input.value) || 0;
      slider.value = v;
      display.textContent = v.toFixed(p.decimals);
    });
  });

  function getValues() {
    const obj = {};
    params.forEach(p => {
      obj[p.key] = parseFloat(document.getElementById(p.input).value) || 0;
    });
    obj.vel_max = 255;
    return obj;
  }

  function log(msg, type = '') {
    const box = document.getElementById('log');
    const now = new Date();
    const ts = now.toTimeString().slice(0,8);
    const div = document.createElement('div');
    div.className = 'entry';
    div.innerHTML = `<span class="ts">${ts}</span><span class="${type}">${msg}</span>`;
    box.prepend(div);
    while (box.children.length > 20) box.removeChild(box.lastChild);
  }

  const dot = document.getElementById('dot');

  async function aplicar() {
    const btn = document.getElementById('btn');
    const values = getValues();

    btn.textContent = '…';
    btn.disabled = true;
    dot.className = 'status-dot sending';

    try {
      const res = await fetch('/set', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(values)
      });

      if (res.ok) {
        btn.textContent = '✓ APLICADO';
        btn.classList.add('success');
        dot.className = 'status-dot online';
        log(`Kp=${values.kp} | Kd=${values.kd} | Base=${values.velocidade_base} | Min=${values.vel_min}`, 'ok');
        setTimeout(() => {
          btn.textContent = '▶ APLICAR';
          btn.classList.remove('success');
        }, 1800);
      } else {
        throw new Error('Resposta ' + res.status);
      }
    } catch (e) {
      btn.textContent = '✗ ERRO';
      btn.classList.add('error');
      dot.className = 'status-dot';
      log('Falha ao enviar: ' + e.message, 'err');
      setTimeout(() => {
        btn.textContent = '▶ APLICAR';
        btn.classList.remove('error');
      }, 2000);
    } finally {
      btn.disabled = false;
    }
  }

  // Carrega valores atuais do servidor ao abrir a página
  async function carregarAtual() {
    try {
      const res = await fetch('/config');
      const data = await res.json();
      params.forEach(p => {
        if (data[p.key] !== undefined) {
          const v = parseFloat(data[p.key]).toFixed(p.decimals);
          document.getElementById(p.slider).value  = v;
          document.getElementById(p.input).value   = v;
          document.getElementById(p.display).textContent = v;
        }
      });
      dot.className = 'status-dot online';
      log('Conectado. Valores carregados.', 'info');
    } catch {
      log('Servidor sem config salva — usando padrões.', '');
    }
  }

  carregarAtual();
</script>
</body>
</html>
"""

# --- ROTAS ---

def ler_config():
    if os.path.exists(CONFIG_PATH):
        with open(CONFIG_PATH, "r") as f:
            return json.load(f)
    return DEFAULT_CONFIG.copy()

def salvar_config(data):
    with open(CONFIG_PATH, "w") as f:
        json.dump(data, f)

@app.route("/")
def index():
    return render_template_string(HTML)

@app.route("/config")
def get_config():
    return jsonify(ler_config())

@app.route("/set", methods=["POST"])
def set_params():
    data = request.get_json()

    config = {
        "kp":             float(data.get("kp",              DEFAULT_CONFIG["kp"])),
        "kd":             float(data.get("kd",              DEFAULT_CONFIG["kd"])),
        "velocidade_base":int(data.get("velocidade_base",   DEFAULT_CONFIG["velocidade_base"])),
        "vel_min":        int(data.get("vel_min",           DEFAULT_CONFIG["vel_min"])),
        "vel_max":        int(data.get("vel_max",           DEFAULT_CONFIG["vel_max"])),
    }

    salvar_config(config)
    print(f"[Web] Novo config: Kp={config['kp']} | Kd={config['kd']} | Base={config['velocidade_base']} | Min={config['vel_min']}")
    return jsonify({"status": "ok", "config": config})


if __name__ == "__main__":
    # Inicializa config padrão se não existir
    if not os.path.exists(CONFIG_PATH):
        salvar_config(DEFAULT_CONFIG)
    print(f"[BuzzLine] Servidor rodando em http://0.0.0.0:5000")
    print(f"[BuzzLine] Config em: {CONFIG_PATH}")
    app.run(host="0.0.0.0", port=5000, debug=False)