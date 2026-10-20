import {
  ESPLoader,
  Transport,
  type FlashFreqValues,
  type FlashModeValues,
  type FlashSizeValues,
  type IEspLoaderTerminal,
} from "esptool-js";
import SparkMD5 from "spark-md5";
import type { FlashStrings } from "./i18n";
import { fetchImage, projectSize } from "./manifest";
import { resetIntoApp } from "./reset";
import type { ManifestProject } from "./types";

export type FlashStage = "port" | "connect" | "download" | "write" | "reset";

export interface FlashRequest {
  project: ManifestProject;
  /** 현재 언어로 표시할 프로젝트 이름 */
  displayName: string;
  eraseAll: boolean;
  verifyMd5: boolean;
  baudrate: number;
  messages: FlashStrings;
  onLog: (line: string) => void;
  onStage: (stage: FlashStage, detail?: string) => void;
  onProgress: (ratio: number, label: string) => void;
  onChip: (chip: string) => void;
}

/** Web Serial 지원 여부 (HTTPS + Chrome/Edge 데스크톱) */
export function isSerialSupported(): boolean {
  return (
    typeof navigator !== "undefined" &&
    window.isSecureContext &&
    typeof navigator.serial?.requestPort === "function"
  );
}

/**
 * Web Serial + esptool-js 로 프로젝트의 4개 이미지를 순서대로 기록한다.
 * - 표준 Arduino IDE 업로드와 동일: bootloader(0x0) / partitions(0x8000) /
 *   boot_app0(0xe000) / app(0x10000)
 * - NVS(0x9000~0xDFFF)는 건드리지 않으므로 기본 설정이 보존된다.
 */
export async function flashProject(request: FlashRequest): Promise<void> {
  const { project, displayName, eraseAll, verifyMd5, baudrate, messages } = request;

  request.onStage("port");
  request.onLog(messages.portPrompt);
  const port = await navigator.serial.requestPort();

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

    request.onStage("download");
    const totalBytes = project.images
      .map((image) => image.size)
      .reduce((sum, size) => sum + size, 0);
    let downloaded = 0;
    const files: { data: Uint8Array; address: number }[] = [];

    for (const image of project.images) {
      const fileName = image.path.split("/").pop() ?? image.path;
      const data = await fetchImage(image);
      downloaded += data.length;
      files.push({ data, address: Number.parseInt(image.offset, 16) });
      request.onProgress(
        totalBytes > 0 ? downloaded / totalBytes : 1,
        messages.preparing(fileName),
      );
      request.onLog(messages.imageReady(image.offset, fileName, data.length));
    }

    request.onStage("write");
    const projectBytes = projectSize(project);
    request.onLog(messages.flashStart(displayName, project.version, files.length, eraseAll));

    await loader.writeFlash({
      fileArray: files,
      flashMode: project.flash.mode.toLowerCase() as FlashModeValues,
      flashFreq: project.flash.freq.toLowerCase() as FlashFreqValues,
      flashSize: project.flash.size.toUpperCase() as FlashSizeValues,
      eraseAll,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const image = project.images[fileIndex];
        if (!image) return;
        const fileName = image.path.split("/").pop() ?? image.path;
        const previous = project.images
          .slice(0, fileIndex)
          .reduce((sum, item) => sum + item.size, 0);
        const ratio = projectBytes > 0
          ? (previous + (written / Math.max(total, 1)) * image.size) / projectBytes
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
