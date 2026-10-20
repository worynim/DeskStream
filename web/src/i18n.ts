import type { ManifestProject } from "./types";

export type Locale = "ko" | "en";
export const LOCALES: Locale[] = ["ko", "en"];

const STORAGE_KEY = "deskstream.flasher.locale";

export interface UiStrings {
  documentTitle: string;
  metaDescription: string;
  heroTitle: string;
  heroLead: string;
  chipLoading: string;
  chipBrowser: string;
  chipError: string;
  warningTitle: string;
  warningBody: string;
  projectsTitle: string;
  projectsAriaLabel: string;
  projectCount: (count: number) => string;
  flashTitle: string;
  selectionEmpty: string;
  verifyLabel: string;
  eraseLabel: string;
  webUsbLabel: string;
  baudLabel: string;
  baudStable: string;
  baudRecommended: string;
  baudFast: string;
  flashStart: string;
  flashBusy: string;
  flashUnsupported: string;
  titleSelectFirst: string;
  titleUnsupported: string;
  progressPreparing: string;
  progressDone: string;
  statusIdle: string;
  statusNoFirmware: string;
  statusLoadError: (message: string) => string;
  statusRetry: string;
  confirmErase: string;
  statusCancelled: string;
  statusConnectFailed: string;
  statusFlashFailed: (message: string) => string;
  statusConnected: (chip: string) => string;
  successMessage: (name: string) => string;
  logHeader: (name: string, version: string) => string;
  logOptions: (
    mode: string,
    freq: string,
    size: string,
    eraseAll: boolean,
    verifyMd5: boolean,
    baud: string,
  ) => string;
  guideHeading: string;
  guide: [string, string, string, string, string, string];
  logSummary: string;
  footerTagline: string;
  manifestMeta: (date: string) => string;
  selectionImages: (count: number, size: string) => string;
  selectionPartition: (chip: string, partition: string) => string;
  selectionBuild: (date: string) => string;
  stagePort: string;
  stageConnect: string;
  stageDownload: string;
  stageWrite: string;
  stageReset: string;
  langAria: string;
  readmeTitle: string;
  readmeAria: (name: string) => string;
}

export interface FlashStrings {
  portPrompt: string;
  webUsbMode: string;
  connecting: string;
  chipDetected: (chip: string) => string;
  chipUnsupported: (chip: string) => string;
  preparing: (file: string) => string;
  imageReady: (offset: string, file: string, bytes: number) => string;
  writing: (current: number, total: number, file: string) => string;
  flashStart: (name: string, version: string, count: number, eraseAll: boolean) => string;
  rebooting: string;
  flashDone: string;
  resetSent: (method: string) => string;
  resetFailed: (error: string) => string;
}

interface LocalePack {
  ui: UiStrings;
  flash: FlashStrings;
}

const ko: LocalePack = {
  ui: {
    documentTitle: "DeskStream 웹 플래셔",
    metaDescription:
      "DeskStream ESP32-C3 보드에 원하는 프로젝트 펌웨어를 브라우저에서 바로 플래시하세요. Chrome/Edge 데스크톱 및 Android Chrome 148+ 지원.",
    heroTitle: "웹 플래셔",
    heroLead:
      "USB 케이블로 ESP32-C3 보드를 연결하고, 원하는 프로젝트를 선택해 브라우저에서 바로 플래시하세요. 별도 프로그램 설치가 필요 없습니다.",
    chipLoading: "프로젝트 불러오는 중…",
    chipBrowser: "Chrome / Edge · Android Chrome",
    chipError: "목록 로드 실패",
    warningTitle: "이 브라우저에서는 플래시할 수 없습니다.",
    warningBody:
      "Web Serial API를 지원하는 <b>Chrome/Edge 데스크톱</b> 또는 <b>Android Chrome 148+</b>에서 <b>HTTPS</b>로 접속해 주세요. iPhone/iPad(Safari)·Firefox Android·인앱 브라우저는 지원하지 않습니다.",
    projectsTitle: "프로젝트 선택",
    projectsAriaLabel: "플래시할 프로젝트 선택",
    projectCount: (count) => `${count}개 프로젝트`,
    flashTitle: "연결 및 플래시",
    selectionEmpty: "위에서 프로젝트를 선택하세요.",
    verifyLabel: "쓰기 검증(MD5) <small>플래시 후 데이터를 다시 읽어 검증합니다</small>",
    eraseLabel: "전체 지우기 <small>NVS의 WiFi·표시 설정까지 삭제됩니다</small>",
    webUsbLabel:
      "USB(WebUSB) 모드 <small>포트 목록에 USB 장치가 안 보일 때 사용 (Android)</small>",
    baudLabel: "통신 속도",
    baudStable: "115200 (안정)",
    baudRecommended: "460800 (권장)",
    baudFast: "921600 (빠름)",
    flashStart: "플래시 시작",
    flashBusy: "플래시 중…",
    flashUnsupported: "지원되지 않는 브라우저",
    titleSelectFirst: "프로젝트를 먼저 선택하세요.",
    titleUnsupported: "Chrome/Edge 데스크톱 또는 Android Chrome 148+에서 사용할 수 있습니다.",
    progressPreparing: "준비 중…",
    progressDone: "완료",
    statusIdle: "프로젝트를 선택한 뒤 USB를 연결하고 플래시를 시작하세요.",
    statusNoFirmware: "사용 가능한 펌웨어가 없습니다.",
    statusLoadError: (message) => `펌웨어 목록을 불러오지 못했습니다: ${message}`,
    statusRetry: "페이지를 새로고침하거나 잠시 후 다시 시도해 주세요.",
    confirmErase:
      "전체 지우기를 실행하면 WiFi·표시 설정을 포함한 NVS 데이터가 모두 삭제됩니다.\n\n계속할까요?",
    statusCancelled: "포트 선택이 취소되었습니다. 다시 시도하려면 플래시 시작을 누르세요.",
    statusConnectFailed:
      "❌ 보드 연결에 실패했습니다. BOOT 버튼을 누른 채 RESET을 눌렀다 놓은 뒤 다시 시도해 주세요. Arduino IDE 시리얼 모니터 등 포트를 사용하는 프로그램도 종료하세요.",
    statusFlashFailed: (message) => `❌ 플래시 실패: ${message}`,
    statusConnected: (chip) => `연결됨 · ${chip}`,
    successMessage: (name) =>
      `✅ ${name} 플래시 완료! 보드가 자동으로 재부팅됩니다. 잠시 기다려도 화면이 켜지지 않으면 RESET 버튼을 한 번 눌러 주세요.`,
    logHeader: (name, version) => `DeskStream 웹 플래셔 · ${name} v${version}`,
    logOptions: (mode, freq, size, eraseAll, verifyMd5, baud) =>
      `flash ${mode}/${freq}/${size} · eraseAll=${eraseAll} · verifyMd5=${verifyMd5} · baud=${baud}`,
    guideHeading: "사용 안내",
    guide: [
      "<b>Chrome/Edge 데스크톱</b> 또는 <b>Android Chrome 148+</b>에서 이 페이지를 엽니다. (iPhone/iPad·Safari·인앱 브라우저 불가)",
      "USB <b>데이터 케이블</b>로 ESP32-C3 보드를 연결합니다. (스마트폰은 <b>USB OTG 어댑터</b> 필요)",
      "프로젝트를 선택하고 <b>플래시 시작</b>을 누른 뒤, 포트 선택 창에서 보드를 고릅니다. USB 장치가 안 보이면 <b>USB(WebUSB) 모드</b>를 켜고 다시 시도하세요.",
      "연결이 되지 않으면 <b>BOOT 버튼을 누른 채 RESET</b>을 눌렀다 놓은 뒤 다시 시도하세요.",
      "Arduino IDE의 시리얼 모니터 등 <b>포트를 점유한 프로그램은 종료</b>해야 합니다.",
      "기본 플래시는 NVS(WiFi·설정)를 보존합니다. 초기화가 필요하면 <b>전체 지우기</b>를 체크하세요.",
    ],
    logSummary: "로그 보기",
    footerTagline: "DeskStream · ESP32-C3 4-OLED 데스크 가젯",
    manifestMeta: (date) => `펌웨어 빌드 ${date} 기준`,
    selectionImages: (count, size) => `${count}개 이미지 · ${size}`,
    selectionPartition: (chip, partition) => `${chip} · ${partition} 파티션`,
    selectionBuild: (date) => `빌드 ${date}`,
    stagePort: "USB 포트를 선택하세요…",
    stageConnect: "보드와 통신을 준비하는 중…",
    stageDownload: "펌웨어를 내려받는 중…",
    stageWrite: "플래시하는 중…",
    stageReset: "보드를 재부팅하는 중…",
    langAria: "언어 선택",
    readmeTitle: "프로젝트 README 열기 (새 탭)",
    readmeAria: (name) => `${name} README 열기 (새 탭)`,
  },
  flash: {
    portPrompt: "포트 선택 창에서 ESP32-C3 보드를 선택하세요.",
    webUsbMode: "WebUSB 모드로 USB 장치를 선택합니다.",
    connecting: "칩과 통신을 설정하는 중…",
    chipDetected: (chip) => `감지된 칩: ${chip}`,
    chipUnsupported: (chip) =>
      `ESP32-C3 보드가 아닙니다 (감지: ${chip}). DeskStream 보드를 연결해 주세요.`,
    preparing: (file) => `펌웨어 준비 중… (${file})`,
    imageReady: (offset, file, bytes) =>
      `이미지 준비: ${offset} ${file} (${bytes.toLocaleString()} bytes)`,
    writing: (current, total, file) => `기록 중… (${current}/${total} · ${file})`,
    flashStart: (name, version, count, eraseAll) =>
      `플래시 시작: ${name} v${version} / ${count}개 이미지 / ${eraseAll ? "전체 지우기" : "설정 보존"}`,
    rebooting: "재부팅 중…",
    flashDone: "플래시 완료. 보드를 재부팅합니다…",
    resetSent: (method) => `재부팅 신호를 보냈습니다 (reset: ${method}).`,
    resetFailed: (error) =>
      `[warn] 자동 재부팅 신호를 보내지 못했습니다. USB를 다시 연결하거나 RESET 버튼을 눌러 주세요. (${error})`,
  },
};

const en: LocalePack = {
  ui: {
    documentTitle: "DeskStream Web Flasher",
    metaDescription:
      "Flash DeskStream project firmware to your ESP32-C3 board directly from the browser. Supports Chrome/Edge on desktop and Chrome 148+ on Android.",
    heroTitle: "Web Flasher",
    heroLead:
      "Connect your ESP32-C3 board over USB, pick a project, and flash it straight from your browser. No extra software to install.",
    chipLoading: "Loading projects…",
    chipBrowser: "Chrome / Edge · Android Chrome",
    chipError: "Failed to load projects",
    warningTitle: "This browser can't flash the board.",
    warningBody:
      "Please use <b>Chrome/Edge on desktop</b> or <b>Chrome 148+ on Android</b> over <b>HTTPS</b>. iPhone/iPad (Safari), Firefox Android, and in-app browsers are not supported.",
    projectsTitle: "Choose a project",
    projectsAriaLabel: "Select a project to flash",
    projectCount: (count) => `${count} projects`,
    flashTitle: "Connect & flash",
    selectionEmpty: "Select a project above.",
    verifyLabel: "Verify write (MD5) <small>Reads the data back after flashing to verify it</small>",
    eraseLabel: "Full erase <small>Also erases WiFi and display settings in NVS</small>",
    webUsbLabel:
      "USB (WebUSB) mode <small>Use if the USB device is missing from the port list (Android)</small>",
    baudLabel: "Baud rate",
    baudStable: "115200 (stable)",
    baudRecommended: "460800 (recommended)",
    baudFast: "921600 (fast)",
    flashStart: "Start flashing",
    flashBusy: "Flashing…",
    flashUnsupported: "Browser not supported",
    titleSelectFirst: "Select a project first.",
    titleUnsupported: "Available in Chrome/Edge on desktop or Chrome 148+ on Android.",
    progressPreparing: "Preparing…",
    progressDone: "Done",
    statusIdle: "Pick a project, connect the board over USB, then start flashing.",
    statusNoFirmware: "No firmware available.",
    statusLoadError: (message) => `Could not load the firmware list: ${message}`,
    statusRetry: "Refresh the page or try again in a moment.",
    confirmErase:
      "Full erase will delete all NVS data, including WiFi and display settings.\n\nContinue?",
    statusCancelled: "Port selection was cancelled. Press Start flashing to try again.",
    statusConnectFailed:
      "❌ Could not connect to the board. Hold BOOT, press and release RESET, then try again. Also close any program using the port, such as the Arduino IDE Serial Monitor.",
    statusFlashFailed: (message) => `❌ Flash failed: ${message}`,
    statusConnected: (chip) => `Connected · ${chip}`,
    successMessage: (name) =>
      `✅ ${name} flashed successfully! The board will reboot automatically. If nothing appears after a few seconds, press RESET once.`,
    logHeader: (name, version) => `DeskStream Web Flasher · ${name} v${version}`,
    logOptions: (mode, freq, size, eraseAll, verifyMd5, baud) =>
      `flash ${mode}/${freq}/${size} · eraseAll=${eraseAll} · verifyMd5=${verifyMd5} · baud=${baud}`,
    guideHeading: "How to use",
    guide: [
      "Open this page in <b>Chrome/Edge on desktop</b> or <b>Chrome 148+ on Android</b>. (iPhone/iPad, Safari, and in-app browsers are not supported.)",
      "Connect the ESP32-C3 board with a USB <b>data cable</b>. (Android phones need a <b>USB OTG adapter</b>.)",
      "Select a project, press <b>Start flashing</b>, then choose the board in the port picker. If the USB device is missing, enable <b>USB (WebUSB) mode</b> and try again.",
      "If it won't connect, hold <b>BOOT</b>, press and release <b>RESET</b>, then try again.",
      "Close any program that is using the port, such as the Arduino IDE Serial Monitor.",
      "Standard flashing preserves NVS (WiFi and settings). Enable <b>Full erase</b> only if you need a clean slate.",
    ],
    logSummary: "Show log",
    footerTagline: "DeskStream · ESP32-C3 4-OLED desk gadget",
    manifestMeta: (date) => `Firmware built ${date}`,
    selectionImages: (count, size) => `${count} images · ${size}`,
    selectionPartition: (chip, partition) => `${chip} · ${partition} partition`,
    selectionBuild: (date) => `Built ${date}`,
    stagePort: "Select the USB port…",
    stageConnect: "Connecting to the board…",
    stageDownload: "Downloading firmware…",
    stageWrite: "Flashing…",
    stageReset: "Rebooting the board…",
    langAria: "Language",
    readmeTitle: "Open project README (new tab)",
    readmeAria: (name) => `Open ${name} README (new tab)`,
  },
  flash: {
    portPrompt: "Select your ESP32-C3 board in the port picker.",
    webUsbMode: "Select the USB device using WebUSB mode.",
    connecting: "Setting up communication with the chip…",
    chipDetected: (chip) => `Detected chip: ${chip}`,
    chipUnsupported: (chip) =>
      `This isn't an ESP32-C3 board (detected: ${chip}). Please connect a DeskStream board.`,
    preparing: (file) => `Preparing firmware… (${file})`,
    imageReady: (offset, file, bytes) =>
      `Image ready: ${offset} ${file} (${bytes.toLocaleString()} bytes)`,
    writing: (current, total, file) => `Writing… (${current}/${total} · ${file})`,
    flashStart: (name, version, count, eraseAll) =>
      `Flashing: ${name} v${version} / ${count} images / ${eraseAll ? "full erase" : "preserving settings"}`,
    rebooting: "Rebooting…",
    flashDone: "Flash complete. Rebooting the board…",
    resetSent: (method) => `Reset signal sent (reset: ${method}).`,
    resetFailed: (error) =>
      `[warn] Could not send the reset signal. Reconnect the USB cable or press RESET. (${error})`,
  },
};

const PACKS: Record<Locale, LocalePack> = { ko, en };

export function getStrings(locale: Locale): LocalePack {
  return PACKS[locale];
}

export function detectLocale(): Locale {
  try {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored === "ko" || stored === "en") return stored;
  } catch {
    // storage unavailable (private mode etc.)
  }
  const language = typeof navigator !== "undefined" ? navigator.language.toLowerCase() : "ko";
  return language.startsWith("ko") ? "ko" : "en";
}

export function storeLocale(locale: Locale): void {
  try {
    localStorage.setItem(STORAGE_KEY, locale);
  } catch {
    // ignore
  }
}

export function dateLocale(locale: Locale): string {
  return locale === "ko" ? "ko-KR" : "en-US";
}

export function projectName(project: ManifestProject, locale: Locale): string {
  return locale === "en" ? project.nameEn || project.name : project.name;
}

export function projectDescription(project: ManifestProject, locale: Locale): string {
  return locale === "en" ? project.descriptionEn || project.description : project.description;
}

export function projectNote(project: ManifestProject, locale: Locale): string {
  return locale === "en" ? project.noteEn || project.note : project.note;
}
