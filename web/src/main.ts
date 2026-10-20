import "./styles.css";
import { readmeUrl } from "./config";
import { flashProject, isSerialSupported, isWebUsbAvailable, type FlashStage } from "./flasher";
import {
  dateLocale,
  detectLocale,
  getStrings,
  projectDescription,
  projectName,
  projectNote,
  storeLocale,
  type Locale,
  type UiStrings,
} from "./i18n";
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
  langSwitch: $("#lang-switch"),
  metaDescription: document.querySelector<HTMLMetaElement>('meta[name="description"]'),
  projectGrid: $("#project-grid"),
  projectCount: $("#project-count"),
  chipProjects: $("#chip-projects"),
  selection: $("#selection"),
  flashButton: $<HTMLButtonElement>("#flash-btn"),
  flashLabel: $("#flash-btn-label"),
  verify: $<HTMLInputElement>("#opt-verify"),
  erase: $<HTMLInputElement>("#opt-erase"),
  webUsbOption: $("#webusb-option"),
  webUsb: $<HTMLInputElement>("#opt-webusb"),
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
  locale: Locale;
  manifest: Manifest | null;
  selected: ManifestProject | null;
  busy: boolean;
  loadErrorMessage: string | null;
} = {
  locale: detectLocale(),
  manifest: null,
  selected: null,
  busy: false,
  loadErrorMessage: null,
};

const serialSupported = isSerialSupported();
const webUsbAvailable = isWebUsbAvailable();
const supported = serialSupported || webUsbAvailable;
const langButtons = [...document.querySelectorAll<HTMLButtonElement>("[data-locale]")];

function ui(): UiStrings {
  return getStrings(state.locale).ui;
}

function formatBytes(bytes: number): string {
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(0)} KB`;
  return `${bytes} B`;
}

function formatDate(iso: string): string {
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return "-";
  return date.toLocaleDateString(dateLocale(state.locale), {
    year: "numeric",
    month: "short",
    day: "numeric",
  });
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
  dom.progressLabel.textContent = ui().progressPreparing;
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

/** 정적 마크업의 data-i18n 계열 속성을 현재 언어로 채운다. */
function applyStaticTranslations(): void {
  const strings = ui();
  document.documentElement.lang = state.locale;
  document.title = strings.documentTitle;
  dom.metaDescription?.setAttribute("content", strings.metaDescription);

  for (const node of document.querySelectorAll<HTMLElement>("[data-i18n]")) {
    const key = node.dataset.i18n;
    if (!key) continue;
    const value = (strings as unknown as Record<string, unknown>)[key];
    if (typeof value === "string") node.textContent = value;
  }

  for (const node of document.querySelectorAll<HTMLElement>("[data-i18n-html]")) {
    const key = node.dataset.i18nHtml;
    if (!key) continue;
    const value = resolveHtml(strings, key);
    if (value !== null) node.innerHTML = value;
  }

  for (const node of document.querySelectorAll<HTMLElement>("[data-i18n-aria-label]")) {
    const key = node.dataset.i18nAriaLabel;
    if (!key) continue;
    const value = (strings as unknown as Record<string, unknown>)[key];
    if (typeof value === "string") node.setAttribute("aria-label", value);
  }

  for (const button of langButtons) {
    button.setAttribute("aria-pressed", String(button.dataset.locale === state.locale));
  }
  dom.langSwitch.setAttribute("aria-label", strings.langAria);
}

function resolveHtml(strings: UiStrings, key: string): string | null {
  if (/^guide[1-6]$/.test(key)) {
    return strings.guide[Number(key.slice(5)) - 1] ?? null;
  }
  const value = (strings as unknown as Record<string, unknown>)[key];
  return typeof value === "string" ? value : null;
}

function renderProjects(projects: ManifestProject[]): void {
  const strings = ui();
  dom.projectGrid.replaceChildren();
  for (const project of [...projects].sort((a, b) => a.order - b.order)) {
    const name = projectName(project, state.locale);

    const item = el("div", "card-item");
    item.dataset.projectId = project.id;

    const card = el("button", "card");
    card.type = "button";
    card.setAttribute("aria-pressed", "false");
    card.title = `${name} v${project.version}`;

    const icon = el("span", "card__icon", project.icon || "📦");
    icon.setAttribute("aria-hidden", "true");

    const body = el("span", "card__body");
    const top = el("span", "card__top");
    top.append(
      el("span", "card__name", name),
      el("span", "card__version", `v${project.version}`),
    );

    const meta = el(
      "span",
      "card__meta",
      `${formatBytes(projectSize(project))} · ${project.partitionScheme ?? "default"} · ${formatDate(project.exportedAt)}`,
    );

    body.append(top, el("span", "card__desc", projectDescription(project, state.locale)), meta);
    card.append(icon, body);
    card.addEventListener("click", () => selectProject(project));

    const readme = el("a", "card__readme");
    readme.href = readmeUrl(project.id);
    readme.target = "_blank";
    readme.rel = "noopener noreferrer";
    readme.title = strings.readmeTitle;
    readme.setAttribute("aria-label", strings.readmeAria(name));
    readme.append(document.createTextNode("README"), el("span", "card__readme-arrow", "↗"));

    item.append(card, readme);
    dom.projectGrid.append(item);
  }
}

function selectProject(project: ManifestProject): void {
  if (state.busy) return;
  state.selected = project;

  for (const item of dom.projectGrid.querySelectorAll<HTMLElement>(".card-item")) {
    const active = item.dataset.projectId === project.id;
    item.classList.toggle("card-item--selected", active);
    const button = item.querySelector<HTMLButtonElement>(".card");
    button?.setAttribute("aria-pressed", active ? "true" : "false");
  }

  renderSelection(project);
  updateControls();
}

function clearSelection(): void {
  state.selected = null;
  dom.selection.classList.add("selection--empty");
  dom.selection.textContent = ui().selectionEmpty;
}

function renderSelection(project: ManifestProject): void {
  const strings = ui();
  dom.selection.classList.remove("selection--empty");
  dom.selection.replaceChildren();

  const head = el("div", "selection__head");
  head.append(
    el("span", "selection__icon", project.icon || "📦"),
    el("strong", "selection__name", projectName(project, state.locale)),
    el("span", "badge", `v${project.version}`),
  );

  const meta = el("div", "selection__meta");
  meta.append(
    el(
      "span",
      undefined,
      strings.selectionImages(project.images.length, formatBytes(projectSize(project))),
    ),
    el(
      "span",
      undefined,
      strings.selectionPartition(
        state.manifest?.chip ?? "ESP32-C3",
        project.partitionScheme ?? "default",
      ),
    ),
    el("span", undefined, strings.selectionBuild(formatDate(project.exportedAt))),
  );

  dom.selection.append(
    head,
    el("p", "selection__desc", projectDescription(project, state.locale)),
    meta,
  );

  const note = projectNote(project, state.locale);
  if (note) {
    dom.selection.append(el("p", "selection__note", `ⓘ ${note}`));
  }
}

function stageLabel(stage: FlashStage): string {
  const strings = ui();
  switch (stage) {
    case "port":
      return strings.stagePort;
    case "connect":
      return strings.stageConnect;
    case "download":
      return strings.stageDownload;
    case "write":
      return strings.stageWrite;
    case "reset":
      return strings.stageReset;
  }
}

function updateControls(): void {
  const strings = ui();
  const canFlash = supported && Boolean(state.selected) && !state.busy;
  dom.flashButton.disabled = !canFlash;

  if (!supported) {
    dom.flashButton.title = strings.titleUnsupported;
    dom.flashLabel.textContent = strings.flashUnsupported;
  } else if (!state.selected) {
    dom.flashButton.title = strings.titleSelectFirst;
    dom.flashLabel.textContent = strings.flashStart;
  } else {
    dom.flashButton.title = "";
    dom.flashLabel.textContent = state.busy ? strings.flashBusy : strings.flashStart;
  }

  dom.verify.disabled = state.busy;
  dom.erase.disabled = state.busy;
  dom.webUsb.disabled = state.busy || !webUsbAvailable || !serialSupported;
  dom.baud.disabled = state.busy;
  for (const button of langButtons) button.disabled = state.busy;
  document.body.classList.toggle("is-busy", state.busy);
}

/** 언어 변경 시 정적 텍스트·프로젝트 카드·선택 패널을 모두 다시 그린다. */
function renderLocale(): void {
  applyStaticTranslations();
  const strings = ui();

  if (state.manifest) {
    const count = state.manifest.projects.length;
    dom.chipProjects.textContent = strings.projectCount(count);
    dom.projectCount.textContent = strings.projectCount(count);
    dom.manifestMeta.textContent = strings.manifestMeta(formatDate(state.manifest.generatedAt));
    renderProjects(state.manifest.projects);

    if (state.selected) {
      const selected =
        state.manifest.projects.find((project) => project.id === state.selected?.id) ?? null;
      if (selected) {
        selectProject(selected);
      } else {
        state.selected = null;
      }
    }
    if (!state.selected) clearSelection();
  } else if (state.loadErrorMessage) {
    dom.chipProjects.textContent = strings.chipError;
    dom.loadError.hidden = false;
    dom.loadError.textContent = strings.statusLoadError(state.loadErrorMessage);
  } else {
    dom.chipProjects.textContent = strings.chipLoading;
  }

  if (!state.busy) {
    setStatus(
      state.manifest
        ? state.manifest.projects.length > 0
          ? strings.statusIdle
          : strings.statusNoFirmware
        : strings.statusRetry,
    );
    resetProgress();
  }
  updateControls();
}

async function handleFlash(): Promise<void> {
  const project = state.selected;
  if (!project || state.busy || !supported) return;

  const strings = ui();
  const flashStrings = getStrings(state.locale).flash;
  const name = projectName(project, state.locale);
  const eraseAll = dom.erase.checked;

  if (eraseAll && !window.confirm(strings.confirmErase)) {
    return;
  }

  state.busy = true;
  updateControls();
  clearLog();
  setProgress(0, strings.stagePort);
  setStatus(strings.stagePort);

  log(strings.logHeader(name, project.version));
  log(
    strings.logOptions(
      project.flash.mode,
      project.flash.freq,
      project.flash.size,
      eraseAll,
      dom.verify.checked,
      dom.baud.value,
    ),
  );

  try {
    await flashProject({
      project,
      displayName: name,
      eraseAll,
      verifyMd5: dom.verify.checked,
      baudrate: Number(dom.baud.value),
      useWebUsb: webUsbAvailable && dom.webUsb.checked,
      messages: flashStrings,
      onLog: log,
      onStage: (stage, detail) => setStatus(detail ?? stageLabel(stage)),
      onProgress: (ratio, label) => setProgress(ratio, label),
      onChip: (chip) => setStatus(strings.statusConnected(chip)),
    });

    setProgress(1, strings.progressDone);
    setStatus(strings.successMessage(name));
    dom.progress.classList.add("progress--done");
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    log(`❌ ${message}`);

    if (/No port selected|No device selected|cancel|취소/i.test(message)) {
      setStatus(strings.statusCancelled);
      resetProgress();
    } else if (/Failed to connect|Failed to communicate|timeout|Timed out/i.test(message)) {
      setStatus(strings.statusConnectFailed);
    } else {
      setStatus(strings.statusFlashFailed(message));
    }
  } finally {
    state.busy = false;
    dom.progress.classList.remove("progress--done");
    updateControls();
  }
}

async function boot(): Promise<void> {
  applyStaticTranslations();

  // WebUSB 우회 모드: WebUSB가 있으면 노출하고, Web Serial이 아예 없으면 기본 활성화
  dom.webUsbOption.hidden = !webUsbAvailable;
  if (!webUsbAvailable) {
    dom.webUsb.checked = false;
  } else if (!serialSupported) {
    dom.webUsb.checked = true;
  }

  if (!supported) dom.browserWarning.hidden = false;

  try {
    state.manifest = await loadManifest();
    state.loadErrorMessage = null;
    renderLocale();
  } catch (error) {
    state.loadErrorMessage = error instanceof Error ? error.message : String(error);
    state.manifest = null;
    renderLocale();
  }

  updateControls();
}

for (const button of langButtons) {
  button.addEventListener("click", () => {
    if (state.busy) return;
    const locale = button.dataset.locale;
    if (locale !== "ko" && locale !== "en") return;
    if (locale === state.locale) return;
    state.locale = locale;
    storeLocale(locale);
    renderLocale();
  });
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
