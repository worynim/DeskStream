import {
  ESPLoader,
  Transport,
  type FlashFreqValues,
  type FlashModeValues,
  type FlashSizeValues,
  type IEspLoaderTerminal,
} from "esptool-js";
import SparkMD5 from "spark-md5";
import { serial as webUsbSerial } from "web-serial-polyfill";
import type { FlashStrings } from "./i18n";
import { fetchImage } from "./manifest";
import {
  findLauncherSlots,
  parsePartitionTable,
  PARTITION_TABLE_OFFSET,
  PARTITION_TABLE_SIZE,
} from "./partitions";
import { resetIntoApp } from "./reset";
import type { ManifestImage, ManifestProject } from "./types";

/** 프로젝트 export 에서 앱·boot_app0(ota_0 선택 otadata) 이미지의 표준 오프셋 */
const APP_OFFSET = 0x10000;
const BOOT_APP0_OFFSET = 0xe000;

interface PlannedImage {
  image: ManifestImage;
  address: number;
}

export type FlashStage = "port" | "connect" | "download" | "write" | "reset";

export interface FlashRequest {
  project: ManifestProject;
  /** 현재 언어로 표시할 프로젝트 이름 */
  displayName: string;
  eraseAll: boolean;
  verifyMd5: boolean;
  baudrate: number;
  /** Web Serial 대신 WebUSB(CDC-ACM)로 포트를 연다 (Android 우회 모드) */
  useWebUsb?: boolean;
  messages: FlashStrings;
  onLog: (line: string) => void;
  onStage: (stage: FlashStage, detail?: string) => void;
  onProgress: (ratio: number, label: string) => void;
  onChip: (chip: string) => void;
}

/** Web Serial 지원 여부 (HTTPS + Chrome/Edge 데스크톱, Android Chrome 148+) */
export function isSerialSupported(): boolean {
  return (
    typeof navigator !== "undefined" &&
    window.isSecureContext &&
    typeof navigator.serial?.requestPort === "function"
  );
}

/** WebUSB(CDC-ACM) 우회 모드 사용 가능 여부 */
export function isWebUsbAvailable(): boolean {
  return (
    typeof navigator !== "undefined" &&
    window.isSecureContext &&
    typeof navigator.usb?.requestDevice === "function"
  );
}

/**
 * Web Serial + esptool-js 로 프로젝트의 4개 이미지를 순서대로 기록한다.
 * - 표준 Arduino IDE 업로드와 동일: bootloader(0x0) / partitions(0x8000) /
 *   boot_app0(0xe000) / app(0x10000)
 * - NVS(0x9000~0xDFFF)는 건드리지 않으므로 기본 설정이 보존된다.
 * - 런처(esp-launcher)가 설치된 기기에 일반 프로젝트를 구우면(전체 지우기 끔)
 *   앱을 ota_0 에, boot_app0 를 otadata 에만 써서 런처를 남긴다.
 */
export async function flashProject(request: FlashRequest): Promise<void> {
  const { project, displayName, eraseAll, verifyMd5, baudrate, messages } = request;

  request.onStage("port");
  request.onLog(request.useWebUsb ? messages.webUsbMode : messages.portPrompt);
  const port = request.useWebUsb
    ? ((await webUsbSerial.requestPort()) as unknown as SerialPort)
    : await navigator.serial.requestPort();

  const transport = new Transport(port, true);
  const terminal: IEspLoaderTerminal = {
    clean: () => undefined,
    write: (data: string) => request.onLog(data.replace(/\n$/, "")),
    writeLine: (data: string) => request.onLog(data.replace(/\n$/, "")),
  };

  const loader = new ESPLoader({
    transport,
    baudrate,
    romBaudrate: 115200,
    terminal,
    debugLogging: false,
  });

  try {
    request.onStage("connect");
    request.onLog(messages.connecting);
    const chip = await loader.main();
    request.onChip(chip);
    request.onLog(messages.chipDetected(chip));

    if (!/ESP32-C3/i.test(chip)) {
      throw new Error(messages.chipUnsupported(chip));
    }

    const plan = await planImages(loader, request);

    request.onStage("download");
    const totalBytes = plan.reduce((sum, { image }) => sum + image.size, 0);
    let downloaded = 0;
    const files: { data: Uint8Array; address: number }[] = [];

    for (const { image, address } of plan) {
      const fileName = image.path.split("/").pop() ?? image.path;
      const data = await fetchImage(image);
      downloaded += data.length;
      files.push({ data, address });
      request.onProgress(
        totalBytes > 0 ? downloaded / totalBytes : 1,
        messages.preparing(fileName),
      );
      request.onLog(messages.imageReady(`0x${address.toString(16)}`, fileName, data.length));
    }

    request.onStage("write");
    request.onLog(messages.flashStart(displayName, project.version, files.length, eraseAll));

    await loader.writeFlash({
      fileArray: files,
      flashMode: project.flash.mode.toLowerCase() as FlashModeValues,
      flashFreq: project.flash.freq.toLowerCase() as FlashFreqValues,
      flashSize: project.flash.size.toUpperCase() as FlashSizeValues,
      eraseAll,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const image = plan[fileIndex]?.image;
        if (!image) return;
        const fileName = image.path.split("/").pop() ?? image.path;
        const previous = plan
          .slice(0, fileIndex)
          .reduce((sum, item) => sum + item.image.size, 0);
        const ratio = totalBytes > 0
          ? (previous + (written / Math.max(total, 1)) * image.size) / totalBytes
          : 0;
        request.onProgress(
          Math.min(1, ratio),
          messages.writing(fileIndex + 1, files.length, fileName),
        );
      },
      calculateMD5Hash: verifyMd5
        ? (image: Uint8Array) => SparkMD5.ArrayBuffer.hash(image.buffer as ArrayBuffer)
        : undefined,
    });

    request.onProgress(1, messages.rebooting);
    request.onStage("reset");
    request.onLog(messages.flashDone);
    try {
      // esptool-js의 after("hard_reset")은 USB-Serial/JTAG에서 리셋되지 않으므로
      // RTC WDT 리셋(C3/C2/S2/S3) 또는 USB-JTAG/클래식 리셋을 사용한다.
      const resetMethod = await resetIntoApp(loader, transport, port);
      request.onLog(messages.resetSent(resetMethod));
    } catch (error) {
      request.onLog(
        messages.resetFailed(error instanceof Error ? error.message : String(error)),
      );
    }
  } finally {
    try {
      await transport.disconnect();
    } catch {
      // 이미 분리된 포트는 무시
    }
  }
}

/**
 * 기록할 이미지와 주소를 정한다.
 * 런처 항목·전체 지우기는 export 그대로, 그 밖에는 기기 파티션 테이블을 읽어 런처가 있으면 앱만 ota_0 에 넣는다.
 */
async function planImages(loader: ESPLoader, request: FlashRequest): Promise<PlannedImage[]> {
  const { project, eraseAll, messages } = request;
  const asExported = project.images.map((image) => ({ image, address: Number.parseInt(image.offset, 16) }));
  if (project.kind === "launcher" || eraseAll) return asExported;

  request.onLog(messages.checkingLauncher);
  const table = await loader.readFlash(PARTITION_TABLE_OFFSET, PARTITION_TABLE_SIZE);
  const slots = findLauncherSlots(parsePartitionTable(table));
  if (!slots) return asExported;

  const app = asExported.find((item) => item.address === APP_OFFSET)?.image;
  const bootApp0 = asExported.find((item) => item.address === BOOT_APP0_OFFSET)?.image;
  if (!app || !bootApp0) throw new Error(messages.launcherImagesMissing);
  if (app.size > slots.ota0.size) throw new Error(messages.launcherAppTooBig(app.size, slots.ota0.size));
  if (bootApp0.size > slots.otadata.size) throw new Error(messages.launcherImagesMissing);

  request.onLog(messages.launcherDetected(`0x${slots.ota0.offset.toString(16)}`));
  return [
    { image: bootApp0, address: slots.otadata.offset },
    { image: app, address: slots.ota0.offset },
  ];
}
