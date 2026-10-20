import "./styles.css";
import { flashProject, isSerialSupported, type FlashStage } from "./flasher";
import { loadManifest, projectSize } from "./manifest";
import type { Manifest, ManifestProject } from "./types";

function $<T extends HTMLElement = HTMLElement>(selector: string): T {
  const element = document.querySelector<T>(selector);
  if (!element) throw new Error(`DOM element not found: ${selector}`);
  return element;
}

function el<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  className?: string,
  text?: string,
): HTMLElementTagNameMap[K] {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

const dom = {
  browserWarning: $("#browser-warning"),
  loadError: $("#load-error"),
  projectGrid: $("#project-grid"),
  projectCount: $("#project-count"),
  chipProjects: $("#chip-projects"),
  selection: $("#selection"),
  flashButton: $<HTMLButtonElement>("#flash-btn"),
  flashLabel: $("#flash-btn-label"),
  verify: $<HTMLInputElement>("#opt-verify"),
  erase: $<HTMLInputElement>("#opt-erase"),
  baud: $<HTMLSelectElement>("#opt-baud"),
  progress: $("#progress"),
  progressFill: $("#progress-fill"),
  progressLabel: $("#progress-label"),
  progressPercent: $("#progress-percent"),
  status: $("#status"),
  log: $("#log"),
  manifestMeta: $("#manifest-meta"),
};

const state: {
  manifest: Manifest | null;
  selected: ManifestProject | null;
  busy: boolean;
} = {
  manifest: null,
  selected: null,
  busy: false,
};

const supported = isSerialSupported();

const STAGE_LABEL: Record<FlashStage, string> = {
  port: "USB 포트를 선택하세요…",
  connect: "보드와 통신을 준비하는 중…",
  download: "펌웨어를 내려받는 중…",
  write: "플래시하는 중…",
  reset: "보드를 재부팅하는 중…",
};

function formatBytes(bytes: number): string {
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(0)} KB`;
  return `${bytes} B`;
}

function formatDate(iso: string): string {
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return "-";
  return date.toLocaleDateString("ko-KR", { year: "numeric", month: "2-digit", day: "2-digit" });
}

function setStatus(text: string): void {
  dom.status.textContent = text;
}

function setProgress(ratio: number, label: string): void {
  const percent = Math.max(0, Math.min(100, Math.round(ratio * 100)));
  dom.progress.hidden = false;
  dom.progressFill.style.width = `${percent}%`;
  dom.progressPercent.textContent = `${percent}%`;
  dom.progressLabel.textContent = label;
}

function resetProgress(): void {
  dom.progress.hidden = true;
  dom.progressFill.style.width = "0%";
  dom.progressPercent.textContent = "0%";
  dom.progressLabel.textContent = "준비 중…";
}

const MAX_LOG_LINES = 500;
function log(line: string): void {
  const text = line.trimEnd();
  if (!text) return;
  dom.log.textContent = `${dom.log.textContent}${text}\n`;
  const lines = dom.log.textContent.split("\n");
  if (lines.length > MAX_LOG_LINES) {
    dom.log.textContent = lines.slice(-MAX_LOG_LINES).join("\n");
  }
  dom.log.scrollTop = dom.log.scrollHeight;
}

function clearLog(): void {
  dom.log.textContent = "";
}

function renderProjects(projects: ManifestProject[]): void {
  dom.projectGrid.replaceChildren();
  for (const project of [...projects].sort((a, b) => a.order - b.order)) {
    const card = el("button", "card");
    card.type = "button";
    card.dataset.projectId = project.id;
    card.setAttribute("role", "option");
    card.setAttribute("aria-selected", "false");
    card.title = `${project.name} v${project.version}`;

    const icon = el("span", "card__icon", project.icon || "📦");
    icon.setAttribute("aria-hidden", "true");

    const body = el("span", "card__body");
    const top = el("span", "card__top");
    top.append(
      el("span", "card__name", project.name),
      el("span", "card__version", `v${project.version}`),
    );

    const meta = el(
      "span",
      "card__meta",
      `${formatBytes(projectSize(project))} · ${project.partitionScheme ?? "default"} · ${formatDate(project.exportedAt)}`,
    );

    body.append(top, el("span", "card__desc", project.description), meta);
    card.append(icon, body);
    card.addEventListener("click", () => selectProject(project));
    dom.projectGrid.append(card);
  }
}

function selectProject(project: ManifestProject): void {
  if (state.busy) return;
  state.selected = project;

  for (const card of dom.projectGrid.querySelectorAll<HTMLButtonElement>(".card")) {
    const active = card.dataset.projectId === project.id;
    card.classList.toggle("card--selected", active);
    card.setAttribute("aria-selected", active ? "true" : "false");
  }

  renderSelection(project);
  updateControls();
}

function renderSelection(project: ManifestProject): void {
  dom.selection.classList.remove("selection--empty");
  dom.selection.replaceChildren();

  const head = el("div", "selection__head");
  head.append(
    el("span", "selection__icon", project.icon || "📦"),
    el("strong", "selection__name", project.name),
    el("span", "badge", `v${project.version}`),
  );

  const meta = el("div", "selection__meta");
  meta.append(
    el("span", undefined, `${project.images.length}개 이미지 · ${formatBytes(projectSize(project))}`),
    el("span", undefined, `${state.manifest?.chip ?? "ESP32-C3"} · ${project.partitionScheme ?? "default"} 파티션`),
    el("span", undefined, `빌드 ${formatDate(project.exportedAt)}`),
  );

  dom.selection.append(head, el("p", "selection__desc", project.description), meta);
  if (project.note) {
    dom.selection.append(el("p", "selection__note", `ⓘ ${project.note}`));
  }
}

function updateControls(): void {
  const canFlash = supported && Boolean(state.selected) && !state.busy;
  dom.flashButton.disabled = !canFlash;

  if (!supported) {
    dom.flashButton.title = "Chrome 또는 Edge 데스크톱에서 사용할 수 있습니다.";
    dom.flashLabel.textContent = "지원되지 않는 브라우저";
  } else if (!state.selected) {
    dom.flashButton.title = "프로젝트를 먼저 선택하세요.";
    dom.flashLabel.textContent = "플래시 시작";
  } else {
    dom.flashButton.title = "";
    dom.flashLabel.textContent = state.busy ? "플래시 중…" : "플래시 시작";
  }

  dom.verify.disabled = state.busy;
  dom.erase.disabled = state.busy;
  dom.baud.disabled = state.busy;
  document.body.classList.toggle("is-busy", state.busy);
}

async function handleFlash(): Promise<void> {
  const project = state.selected;
  if (!project || state.busy || !supported) return;

  const eraseAll = dom.erase.checked;
  if (
    eraseAll &&
    !window.confirm(
      "전체 지우기를 실행하면 WiFi·표시 설정을 포함한 NVS 데이터가 모두 삭제됩니다.\n\n계속할까요?",
    )
  ) {
    return;
  }

  state.busy = true;
  updateControls();
  clearLog();
  setProgress(0, STAGE_LABEL.port);
  setStatus("포트 선택 창에서 ESP32-C3 보드를 선택하세요.");

  log(`DeskStream 웹 플래셔 · ${project.name} v${project.version}`);
  log(
    `flash ${project.flash.mode}/${project.flash.freq}/${project.flash.size} · ` +
      `eraseAll=${eraseAll} · verifyMd5=${dom.verify.checked} · baud=${dom.baud.value}`,
  );

  try {
    await flashProject({
      project,
      eraseAll,
      verifyMd5: dom.verify.checked,
      baudrate: Number(dom.baud.value),
      onLog: log,
      onStage: (stage, detail) => setStatus(detail ?? STAGE_LABEL[stage]),
      onProgress: (ratio, label) => setProgress(ratio, label),
      onChip: (chip) => setStatus(`연결됨 · ${chip}`),
    });

    setProgress(1, "완료");
    setStatus(`✅ ${project.name} 플래시 완료! 보드가 재부팅됩니다.`);
    dom.progress.classList.add("progress--done");
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    log(`❌ ${message}`);

    if (/No port selected|cancel|취소/i.test(message)) {
      setStatus("포트 선택이 취소되었습니다. 다시 시도하려면 플래시 시작을 누르세요.");
      resetProgress();
    } else if (/Failed to connect|Failed to communicate|timeout|Timed out/i.test(message)) {
      setStatus(
        "❌ 보드 연결에 실패했습니다. BOOT 버튼을 누른 채 RESET을 눌렀다 놓은 뒤 다시 시도해 주세요. " +
          "Arduino IDE 시리얼 모니터 등 포트를 사용하는 프로그램도 종료하세요.",
      );
    } else {
      setStatus(`❌ 플래시 실패: ${message}`);
    }
  } finally {
    state.busy = false;
    dom.progress.classList.remove("progress--done");
    updateControls();
  }
}

async function boot(): Promise<void> {
  if (!supported) dom.browserWarning.hidden = false;

  try {
    const manifest = await loadManifest();
    state.manifest = manifest;
    dom.chipProjects.textContent = `${manifest.projects.length}개 프로젝트`;
    dom.projectCount.textContent = `${manifest.projects.length}개`;
    dom.manifestMeta.textContent = `펌웨어 빌드 ${formatDate(manifest.generatedAt)} 기준`;
    renderProjects(manifest.projects);
    setStatus(
      manifest.projects.length > 0
        ? "프로젝트를 선택한 뒤 USB를 연결하고 플래시를 시작하세요."
        : "사용 가능한 펌웨어가 없습니다.",
    );
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    dom.loadError.hidden = false;
    dom.loadError.textContent = `펌웨어 목록을 불러오지 못했습니다: ${message}`;
    dom.chipProjects.textContent = "목록 로드 실패";
    setStatus("페이지를 새로고침하거나 잠시 후 다시 시도해 주세요.");
  }

  updateControls();
}

dom.flashButton.addEventListener("click", () => {
  void handleFlash();
});

window.addEventListener("beforeunload", (event) => {
  if (!state.busy) return;
  event.preventDefault();
  event.returnValue = "";
});

void boot();
