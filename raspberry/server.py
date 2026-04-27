import os
import json
import time
from flask import Flask, request, jsonify, render_template_string

app = Flask(__name__)

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "buzzline_config.json")

# Adicionado erro_limiar_curva ao DEFAULT_CONFIG
DEFAULT_CONFIG = {
    "kp": 0.5,
    "kd": 0.0,
    "velocidade_base": 130,
    "vel_min": 100,
    "vel_max": 255,
    "erro_limiar_curva": 120
}

# --- HTML da Interface (Atualizado com novo Slider) ---
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
  }
  header { width: 100%; max-width: 560px; display: flex; align-items: flex-end; gap: 16px; margin-bottom: 32px; padding-bottom: 20px; border-bottom: 1px solid var(--border); }
  .logo { font-family: var(--mono); font-size: 1.5rem; color: var(--accent); letter-spacing: 0.08em; line-height: 1; }
  .logo span { color: var(--accent2); }
  .subtitle { font-size: 0.72rem; color: var(--muted); letter-spacing: 0.12em; text-transform: uppercase; font-weight: 600; margin-bottom: 2px; }
  .status-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--muted); margin-left: auto; margin-bottom: 4px; transition: background 0.3s; }
  .status-dot.online { background: #00e676; box-shadow: 0 0 8px #00e676; }
  .status-dot.sending { background: var(--warn); box-shadow: 0 0 8px var(--warn); }
  .card { width: 100%; max-width: 560px; background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 28px; margin-bottom: 16px; }
  .card-title { font-family: var(--mono); font-size: 0.7rem; color: var(--muted); letter-spacing: 0.2em; text-transform: uppercase; margin-bottom: 24px; display: flex; align-items: center; gap: 10px; }
  .card-title::after { content: ''; flex: 1; height: 1px; background: var(--border); }
  .param-row { margin-bottom: 28px; }
  .param-header { display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 10px; }
  .param-label { font-family: var(--mono); font-size: 0.9rem; color: var(--text); }
  .param-label em { font-style: normal; font-size: 0.72rem; color: var(--muted); margin-left: 8px; }
  .param-value { font-family: var(--mono); font-size: 1.2rem; color: var(--accent); min-width: 60px; text-align: right; }
  .slider-wrap { display: flex; align-items: center; gap: 12px; }
  input[type=range] { -webkit-appearance: none; flex: 1; height: 3px; background: var(--border); border-radius: 2px; outline: none; }
  input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 18px; height: 18px; border-radius: 50%; background: var(--accent); border: 2px solid var(--bg); cursor: pointer; }
  input[type=number] { width: 72px; background: var(--bg); border: 1px solid var(--border); border-radius: 4px; color: var(--accent); font-family: var(--mono); font-size: 0.85rem; padding: 5px 8px; text-align: center; }
  .btn-apply { width: 100%; max-width: 560px; height: 52px; background: transparent; border: 1px solid var(--accent); border-radius: 6px; color: var(--accent); font-family: var(--mono); text-transform: uppercase; cursor: pointer; transition: 0.2s; margin-bottom: 12px; }
  .btn-apply:hover { background: rgba(0,229,255,0.08); }
  .log-box { width: 100%; max-width: 560px; background: var(--bg); border: 1px solid var(--border); border-radius: 6px; padding: 14px 16px; font-family: var(--mono); font-size: 0.72rem; color: var(--muted); max-height: 130px; overflow-y: auto; }
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
      <input type="number" id="kp-input" min="0" max="5" step="0.05" value="0.50">
    </div>
  </div>
  <div class="param-row">
    <div class="param-header">
      <span class="param-label">Kd <em>derivativo</em></span>
      <span class="param-value" id="kd-display" style="color:var(--accent2)">0.00</span>
    </div>
    <div class="slider-wrap">
      <input type="range" id="kd-slider" min="0" max="10" step="0.1" value="0.0">
      <input type="number" id="kd-input" min="0" max="10" step="0.1" value="0.00">
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">Configurações de Curva (4x4)</div>
  <div class="param-row">
    <div class="param-header">
      <span class="param-label">Limiar de Travamento <em>(Erro)</em></span>
      <span class="param-value" id="limiar-display" style="color:var(--warn)">120</span>
    </div>
    <div class="slider-wrap">
      <input type="range" id="limiar-slider" min="50" max="300" step="1" value="120">
      <input type="number" id="limiar-input" min="50" max="300" step="1" value="120">
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">Velocidade</div>
  <div class="param-row">
    <div class="param-header">
      <span class="param-label">Base <em>linha reta</em></span>
      <span class="param-value" id="vbase-display" style="color:var(--warn)">130</span>
    </div>
    <div class="slider-wrap">
      <input type="range" id="vbase-slider" min="80" max="255" step="1" value="130">
      <input type="number" id="vbase-input" min="80" max="255" step="1" value="130">
    </div>
  </div>
</div>

<button class="btn-apply" id="btn" onclick="aplicar()">▶ APLICAR</button>
<div class="log-box" id="log"></div>

<script>
  const params = [
    { key: 'kp',             slider: 'kp-slider',    input: 'kp-input',    display: 'kp-display',    decimals: 2 },
    { key: 'kd',             slider: 'kd-slider',    input: 'kd-input',    display: 'kd-display',    decimals: 2 },
    { key: 'velocidade_base',slider: 'vbase-slider', input: 'vbase-input', display: 'vbase-display', decimals: 0 },
    { key: 'erro_limiar_curva', slider: 'limiar-slider', input: 'limiar-input', display: 'limiar-display', decimals: 0 },
  ];

  params.forEach(p => {
    const slider  = document.getElementById(p.slider);
    const input   = document.getElementById(p.input);
    const display = document.getElementById(p.display);
    slider.addEventListener('input', () => {
      const v = parseFloat(slider.value).toFixed(p.decimals);
      input.value = v; display.textContent = v;
    });
    input.addEventListener('input', () => {
      const v = parseFloat(input.value) || 0;
      slider.value = v; display.textContent = v.toFixed(p.decimals);
    });
  });

  async function aplicar() {
    const btn = document.getElementById('btn');
    const values = {};
    params.forEach(p => values[p.key] = parseFloat(document.getElementById(p.input).value) || 0);
    values.vel_min = 100; values.vel_max = 255;
    
    try {
      const res = await fetch('/set', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(values)
      });
      if (res.ok) { document.getElementById('dot').className = 'status-dot online'; }
    } catch (e) { document.getElementById('dot').className = 'status-dot'; }
  }

  async function carregarAtual() {
    try {
      const res = await fetch('/config');
      const data = await res.json();
      params.forEach(p => {
        if (data[p.key] !== undefined) {
          const v = parseFloat(data[p.key]).toFixed(p.decimals);
          document.getElementById(p.slider).value = v;
          document.getElementById(p.input).value = v;
          document.getElementById(p.display).textContent = v;
        }
      });
    } catch {}
  }
  carregarAtual();
</script>
</body>
</html>
"""

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
        "kp": float(data.get("kp", DEFAULT_CONFIG["kp"])),
        "kd": float(data.get("kd", DEFAULT_CONFIG["kd"])),
        "velocidade_base": int(data.get("velocidade_base", DEFAULT_CONFIG["velocidade_base"])),
        "vel_min": int(data.get("vel_min", 100)),
        "vel_max": int(data.get("vel_max", 255)),
        "erro_limiar_curva": int(data.get("erro_limiar_curva", DEFAULT_CONFIG["erro_limiar_curva"])),
    }
    salvar_config(config)
    return jsonify({"status": "ok", "config": config})

if __name__ == "__main__":
    if not os.path.exists(CONFIG_PATH):
        salvar_config(DEFAULT_CONFIG)
    app.run(host="0.0.0.0", port=5000, debug=False)