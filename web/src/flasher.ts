import {
  ESPLoader,
  Transport,
  type FlashFreqValues,
  type FlashModeValues,
  type FlashSizeValues,
  type IEspLoaderTerminal,
} from "esptool-js";
import SparkMD5 from "spark-md5";
import { fetchImage, projectSize } from "./manifest";
import { resetIntoApp } from "./reset";
import type { ManifestProject } from "./types";

export type FlashStage = "port" | "connect" | "download" | "write" | "reset";

export interface FlashRequest {
  project: ManifestProject;
  eraseAll: boolean;
  verifyMd5: boolean;
  baudrate: number;
  onLog: (line: string) => void;
  onStage: (stage: FlashStage, detail?: string) => void;
  onProgress: (ratio: number, label: string) => void;
  onChip: (chip: string) => void;
}

/** Web Serial 지원 여부 (HTTPS + Chrome/Edge 데스크톱) */
export function isSerialSupported(): boolean {
  return typeof navigator !== "undefined" && "serial" in navigator && window.isSecureContext;
}

/**
 * Web Serial + esptool-js 로 프로젝트의 4개 이미지를 순서대로 기록한다.
 * - 표준 Arduino IDE 업로드와 동일: bootloader(0x0) / partitions(0x8000) /
 *   boot_app0(0xe000) / app(0x10000)
 * - NVS(0x9000~0xDFFF)는 건드리지 않으므로 기본 설정이 보존된다.
 */
export async function flashProject(request: FlashRequest): Promise<void> {
  const { project, eraseAll, verifyMd5, baudrate } = request;

  request.onStage("port");
  request.onLog("포트 선택 창에서 ESP32-C3 보드를 선택하세요.");
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
    request.onLog("칩과 통신을 설정하는 중…");
    const chip = await loader.main();
    request.onChip(chip);
    request.onLog(`감지된 칩: ${chip}`);

    if (!/ESP32-C3/i.test(chip)) {
      throw new Error(`ESP32-C3 보드가 아닙니다 (감지: ${chip}). DeskStream 보드를 연결해 주세요.`);
    }

    request.onStage("download");
    const totalBytes = project.images
      .map((image) => image.size)
      .reduce((sum, size) => sum + size, 0);
    let downloaded = 0;
    const files: { data: Uint8Array; address: number }[] = [];

    for (const image of project.images) {
      const data = await fetchImage(image);
      downloaded += data.length;
      files.push({ data, address: Number.parseInt(image.offset, 16) });
      request.onProgress(
        totalBytes > 0 ? downloaded / totalBytes : 1,
        `펌웨어 준비 중… (${image.path.split("/").pop()})`,
      );
      request.onLog(`이미지 준비: ${image.offset} ${image.path.split("/").pop()} (${data.length.toLocaleString()} bytes)`);
    }

    request.onStage("write");
    const projectBytes = projectSize(project);
    request.onLog(
      `플래시 시작: ${project.name} v${project.version} / ${files.length}개 이미지 / ${eraseAll ? "전체 지우기" : "설정 보존"}`,
    );

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
        const previous = project.images
          .slice(0, fileIndex)
          .reduce((sum, item) => sum + item.size, 0);
        const ratio = projectBytes > 0
          ? (previous + (written / Math.max(total, 1)) * image.size) / projectBytes
          : 0;
        request.onProgress(
          Math.min(1, ratio),
          `기록 중… (${fileIndex + 1}/${files.length} · ${image.path.split("/").pop()})`,
        );
      },
      calculateMD5Hash: verifyMd5
        ? (image: Uint8Array) => SparkMD5.ArrayBuffer.hash(image.buffer as ArrayBuffer)
        : undefined,
    });

    request.onProgress(1, "재부팅 중…");
    request.onStage("reset");
    request.onLog("플래시 완료. 보드를 재부팅합니다…");
    try {
      // esptool-js의 after("hard_reset")은 USB-Serial/JTAG에서 리셋되지 않으므로
      // RTC WDT 리셋(C3/C2/S2/S3) 또는 USB-JTAG/클래식 리셋을 사용한다.
      const resetMethod = await resetIntoApp(loader, transport, port);
      request.onLog(`재부팅 신호를 보냈습니다 (reset: ${resetMethod}).`);
    } catch (error) {
      request.onLog(
        `[warn] 자동 재부팅 신호를 보내지 못했습니다. USB를 다시 연결하거나 RESET 버튼을 눌러 주세요. (${error instanceof Error ? error.message : String(error)})`,
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
