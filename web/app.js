const SIZE = 8;
const presets = {
  diamond: [
    "00011000", "00111100", "01111110", "11111111",
    "11111111", "01111110", "00111100", "00011000",
  ],
  square: [
    "11111111", "11111111", "11111111", "11111111",
    "11111111", "11111111", "11111111", "11111111",
  ],
  cutCorners: [
    "01111111", "11111111", "11111111", "11111111",
    "11111111", "11111111", "11111111", "11111110",
  ],
};

const TILE = [
  [0,0,0,0,0,0,0,0],[0,0,0,1,0,0,0,1],[0,0,1,0,0,0,1,0],[0,0,1,0,0,1,1,1],
  [0,0,1,0,1,1,0,1],[0,0,1,1,1,0,0,1],[0,1,0,0,0,1,0,0],[0,1,0,0,1,0,1,1],
  [0,1,0,0,1,1,1,0],[0,1,0,1,0,1,0,1],[0,1,0,1,1,0,1,0],[0,1,0,1,1,1,1,1],
  [0,1,1,0,1,0,0,1],[0,1,1,1,0,0,1,0],[0,1,1,1,0,1,1,1],[0,1,1,1,1,1,0,1],
  [1,0,0,0,1,0,0,0],[1,0,0,1,0,0,1,1],[1,0,0,1,0,1,1,0],[1,0,0,1,1,1,0,0],
  [1,0,1,0,0,1,0,1],[1,0,1,0,1,0,1,0],[1,0,1,0,1,1,1,1],[1,0,1,1,0,1,0,0],
  [1,0,1,1,1,0,1,1],[1,0,1,1,1,1,1,0],[1,1,0,0,1,0,0,1],[1,1,0,1,0,0,1,0],
  [1,1,0,1,0,1,1,1],[1,1,0,1,1,1,0,1],[1,1,1,0,0,1,0,0],[1,1,1,0,1,0,1,1],
  [1,1,1,0,1,1,1,0],[1,1,1,1,0,1,0,1],[1,1,1,1,1,0,1,0],[1,1,1,1,1,1,1,1],
];
const DIRS = [[0,-1],[1,-1],[1,0],[1,1],[0,1],[-1,1],[-1,0],[-1,-1]];

const elements = {
  grid: document.querySelector("#dot-grid"),
  cells: document.querySelector("#cell-count"),
  border: document.querySelector("#border-count"),
  circuit: document.querySelector("#circuit-length"),
  circuitMetric: document.querySelector(".metric-circuit"),
  condition: document.querySelector("#condition-pill"),
  status: document.querySelector("#runtime-status"),
  solve: document.querySelector("#solve-button"),
  density: document.querySelector("#search-density"),
  guidance: document.querySelector("#guidance"),
  placeholder: document.querySelector("#result-placeholder"),
  svg: document.querySelector("#crease-pattern"),
  overlay: document.querySelector("#solving-overlay"),
  actions: document.querySelector("#result-actions"),
  download: document.querySelector("#download-button"),
  share: document.querySelector("#share-button"),
  undo: document.querySelector("#undo-button"),
  rotate: document.querySelector("#rotate-button"),
  clear: document.querySelector("#clear-button"),
  log: document.querySelector("#search-log"),
  logContent: document.querySelector("#search-log-content"),
  toast: document.querySelector("#toast"),
};

let dots = Array(SIZE * SIZE).fill(0);
let undoStack = [];
let engineReady = false;
let circuitLength = null;
let borderLength = 0;
let analysisTimer;
let requestId = 0;
let latestAnalysis = 0;
let drawValue = null;
let toastTimer;

const worker = new Worker("./solver-worker.js");
const pending = new Map();

worker.onmessage = ({ data }) => {
  if (data.type === "ready") {
    engineReady = true;
    elements.status.className = "runtime-status ready";
    elements.status.lastElementChild.textContent = "計算エンジン準備完了";
    scheduleAnalysis(0);
    return;
  }
  if (data.type === "error" && !data.id) {
    elements.status.className = "runtime-status error";
    elements.status.lastElementChild.textContent = "計算エンジンの読込に失敗";
    elements.guidance.textContent = "ページを再読み込みしてください。";
    elements.guidance.className = "guidance error";
    return;
  }
  const callback = pending.get(data.id);
  if (callback) {
    pending.delete(data.id);
    data.type === "error" ? callback.reject(new Error(data.error)) : callback.resolve(data);
  }
};

function askWorker(type, payload = {}) {
  const id = ++requestId;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    worker.postMessage({ id, type, ...payload });
  });
}

function dotsToString() { return dots.join(""); }

function stringToDots(value) {
  if (!/^[01]{64}$/.test(value)) return null;
  return [...value].map(Number);
}

function fromHash() {
  const match = location.hash.match(/^#p=([01]{64})$/);
  return match ? stringToDots(match[1]) : null;
}

function buildGrid() {
  const fragment = document.createDocumentFragment();
  for (let i = 0; i < SIZE * SIZE; i += 1) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "dot-cell";
    button.dataset.index = String(i);
    button.setAttribute("role", "gridcell");
    button.setAttribute("aria-label", `${Math.floor(i / SIZE) + 1}行${(i % SIZE) + 1}列`);
    button.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      pushHistory();
      drawValue = dots[i] ? 0 : 1;
      paint(i, drawValue);
      button.setPointerCapture(event.pointerId);
    });
    button.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        pushHistory();
        paint(i, dots[i] ? 0 : 1);
        finishEdit();
      }
    });
    fragment.append(button);
  }
  elements.grid.append(fragment);
}

elements.grid.addEventListener("pointermove", (event) => {
  if (drawValue === null) return;
  const target = document.elementFromPoint(event.clientX, event.clientY)?.closest(".dot-cell");
  if (target && elements.grid.contains(target)) paint(Number(target.dataset.index), drawValue);
});
window.addEventListener("pointerup", () => {
  if (drawValue !== null) finishEdit();
  drawValue = null;
});

function paint(index, value) {
  if (dots[index] === value) return;
  dots[index] = value;
  renderGrid();
  updateLocalMetrics();
}

function pushHistory() {
  undoStack.push([...dots]);
  if (undoStack.length > 40) undoStack.shift();
  elements.undo.disabled = false;
}

function finishEdit() {
  hideResult();
  window.history.replaceState(null, "", `#p=${dotsToString()}`);
  scheduleAnalysis();
}

function renderGrid() {
  [...elements.grid.children].forEach((cell, index) => {
    cell.classList.toggle("active", dots[index] === 1);
    cell.setAttribute("aria-pressed", dots[index] ? "true" : "false");
  });
}

function updateLocalMetrics() {
  const active = dots.reduce((sum, value) => sum + value, 0);
  let shared = 0;
  for (let y = 0; y < SIZE; y += 1) {
    for (let x = 0; x < SIZE; x += 1) {
      const i = y * SIZE + x;
      if (!dots[i]) continue;
      if (x < SIZE - 1 && dots[i + 1]) shared += 1;
      if (y < SIZE - 1 && dots[i + SIZE]) shared += 1;
    }
  }
  borderLength = active * 4 - shared * 2;
  elements.cells.textContent = String(active);
  elements.border.textContent = String(borderLength);
  circuitLength = null;
  elements.circuit.textContent = "…";
  elements.circuitMetric.classList.remove("valid");
  elements.solve.disabled = true;
  elements.condition.textContent = "解析中";
  elements.condition.className = "condition-pill";
}

function scheduleAnalysis(delay = 180) {
  clearTimeout(analysisTimer);
  if (!engineReady) return;
  const token = ++latestAnalysis;
  analysisTimer = setTimeout(async () => {
    try {
      const { value } = await askWorker("analyze", { dots: dotsToString() });
      if (token !== latestAnalysis) return;
      circuitLength = value >= 2147483647 || value < 0 ? Infinity : value;
      updateCondition();
    } catch (error) {
      elements.guidance.textContent = `解析できませんでした: ${error.message}`;
      elements.guidance.className = "guidance error";
    }
  }, delay);
}

function updateCondition(preserveGuidance = false) {
  const valid = circuitLength === 32;
  const connected = circuitLength === borderLength;
  elements.circuit.textContent = Number.isFinite(circuitLength) ? String(circuitLength) : "不可";
  elements.circuitMetric.classList.toggle("valid", valid);
  elements.condition.className = `condition-pill ${valid ? "valid" : "invalid"}`;
  elements.condition.textContent = valid ? "回路長 32 ✓" : "回路長 32 が必要";
  elements.solve.disabled = !(engineReady && valid && connected);
  if (!preserveGuidance) {
    elements.guidance.className = "guidance";
    if (!valid) {
      elements.guidance.textContent = "回路長が 32 になるようにマスを調整してください。";
    } else if (!connected) {
      elements.guidance.textContent = "現在のWeb版では、ひとつながりの図形を入力してください。";
    } else {
      elements.guidance.textContent = "条件を満たしました。展開図を探索できます。";
      elements.guidance.className = "guidance success";
    }
  }
}

function setPattern(pattern, save = true) {
  const next = Array.isArray(pattern) && typeof pattern[0] === "string"
    ? pattern.join("").split("").map(Number)
    : [...pattern];
  if (save) pushHistory();
  dots = next;
  renderGrid();
  updateLocalMetrics();
  finishEdit();
}

function hideResult() {
  elements.svg.hidden = true;
  elements.placeholder.hidden = false;
  elements.actions.hidden = true;
  elements.log.hidden = true;
}

function parseSolution(output) {
  const cp = output.match(/CPSTR:(\d+)/)?.[1];
  const corners = output.match(/CORNERS:\s*([01])\s+([01])\s+([01])\s+([01])/)?.slice(1).map(Number);
  return cp && corners ? { cp, corners } : null;
}

function svgNode(name, attributes = {}) {
  const node = document.createElementNS("http://www.w3.org/2000/svg", name);
  Object.entries(attributes).forEach(([key, value]) => node.setAttribute(key, value));
  return node;
}

function renderCreasePattern(cpstr, corners) {
  elements.svg.replaceChildren();
  elements.svg.append(svgNode("rect", { class: "paper", x: 0, y: 0, width: 8, height: 8 }));
  for (let i = 1; i < 8; i += 1) {
    elements.svg.append(svgNode("line", { class: "guide", x1: i, y1: 0, x2: i, y2: 8 }));
    elements.svg.append(svgNode("line", { class: "guide", x1: 0, y1: i, x2: 8, y2: i }));
  }

  const tiles = cpstr.match(/.{2}/g).map(Number);
  const seen = new Set();
  const addCrease = (x1, y1, x2, y2) => {
    const a = `${x1},${y1}`;
    const b = `${x2},${y2}`;
    const key = a < b ? `${a}|${b}` : `${b}|${a}`;
    if (seen.has(key)) return;
    seen.add(key);
    const diagonal = x1 !== x2 && y1 !== y2;
    elements.svg.append(svgNode("line", {
      class: `crease${diagonal ? " diagonal" : ""}`,
      x1, y1, x2, y2,
    }));
  };

  tiles.forEach((tile, index) => {
    const x = (index % 7) + 1;
    const y = Math.floor(index / 7) + 1;
    TILE[tile].forEach((use, direction) => {
      if (!use) return;
      const [dx, dy] = DIRS[direction];
      addCrease(x, y, x + dx, y + dy);
    });
  });

  const cornerEdges = [
    [1,0,0,1], [7,0,8,1], [8,7,7,8], [0,7,1,8],
  ];
  corners.forEach((use, index) => { if (use) addCrease(...cornerEdges[index]); });
  elements.svg.append(svgNode("rect", { class: "boundary", x: 0, y: 0, width: 8, height: 8 }));
  elements.placeholder.hidden = true;
  elements.svg.hidden = false;
  elements.actions.hidden = false;
}

async function solve() {
  if (elements.solve.disabled) return;
  elements.solve.disabled = true;
  elements.overlay.hidden = false;
  elements.guidance.textContent = "候補の折り線を検証しています…";
  elements.guidance.className = "guidance";
  const started = performance.now();
  try {
    const { output } = await askWorker("solve", {
      dots: dotsToString(),
      skip: Number(elements.density.value),
    });
    const elapsed = ((performance.now() - started) / 1000).toFixed(2);
    elements.logContent.textContent = `${output.trim()}\nBrowser time: ${elapsed}s`;
    elements.log.hidden = false;
    const solution = parseSolution(output);
    if (solution) {
      renderCreasePattern(solution.cp, solution.corners);
      elements.guidance.textContent = `${elapsed}秒で、平坦に折れる展開図が見つかりました。`;
      elements.guidance.className = "guidance success";
    } else {
      hideResult();
      elements.log.hidden = false;
      elements.guidance.textContent = "この探索精度では展開図が見つかりませんでした。精度を上げて再度お試しください。";
      elements.guidance.className = "guidance error";
    }
  } catch (error) {
    elements.guidance.textContent = `探索中にエラーが発生しました: ${error.message}`;
    elements.guidance.className = "guidance error";
  } finally {
    elements.overlay.hidden = true;
    updateCondition(true);
  }
}

function downloadSvg() {
  const clone = elements.svg.cloneNode(true);
  clone.setAttribute("xmlns", "http://www.w3.org/2000/svg");
  const style = document.createElementNS("http://www.w3.org/2000/svg", "style");
  style.textContent = ".paper{fill:#fffefb}.guide{stroke:#dfe3df;stroke-width:.015;stroke-dasharray:.05 .08}.crease{stroke:#294e68;stroke-width:.055;stroke-linecap:round}.crease.diagonal{stroke:#e15f42}.boundary{fill:none;stroke:#1d2a30;stroke-width:.11;stroke-linejoin:round}";
  clone.prepend(style);
  const blob = new Blob([new XMLSerializer().serializeToString(clone)], { type: "image/svg+xml" });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = "dot-fold-pattern.svg";
  anchor.click();
  URL.revokeObjectURL(url);
}

function showToast(message) {
  clearTimeout(toastTimer);
  elements.toast.textContent = message;
  elements.toast.classList.add("show");
  toastTimer = setTimeout(() => elements.toast.classList.remove("show"), 2200);
}

elements.undo.addEventListener("click", () => {
  const previous = undoStack.pop();
  if (!previous) return;
  dots = previous;
  elements.undo.disabled = undoStack.length === 0;
  renderGrid(); updateLocalMetrics(); finishEdit();
});
elements.rotate.addEventListener("click", () => {
  pushHistory();
  const rotated = Array(64).fill(0);
  for (let y = 0; y < 8; y += 1) for (let x = 0; x < 8; x += 1) rotated[x * 8 + (7 - y)] = dots[y * 8 + x];
  setPattern(rotated, false);
});
elements.clear.addEventListener("click", () => setPattern(Array(64).fill(0)));
document.querySelectorAll("[data-preset]").forEach((button) => button.addEventListener("click", () => setPattern(presets[button.dataset.preset])));
elements.solve.addEventListener("click", solve);
elements.download.addEventListener("click", downloadSvg);
elements.share.addEventListener("click", async () => {
  const url = `${location.origin}${location.pathname}#p=${dotsToString()}`;
  try { await navigator.clipboard.writeText(url); showToast("URLをコピーしました"); }
  catch { prompt("このURLをコピーしてください", url); }
});

buildGrid();
elements.undo.disabled = true;
dots = fromHash() ?? presets.diamond.join("").split("").map(Number);
renderGrid();
updateLocalMetrics();
window.history.replaceState(null, "", `#p=${dotsToString()}`);
