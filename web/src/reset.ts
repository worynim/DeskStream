import { ESPLoader, Transport, UsbJtagSerialReset } from "esptool-js";

/**
 * 플래시 완료 후 애플리케이션으로 재부팅시키는 리셋 전략.
 *
 * esptool-js 0.7.0의 `after("hard_reset")`은 비 USB-OTG 기기에서
 * `setRTS(false)`만 호출한다. ESP32-C3의 내장 USB-Serial/JTAG는 이 동작으로
 * 리셋되지 않아 펌웨어가 부팅되지 않고 stub에 머무른다.
 * → esptool(Python)의 `--after watchdog-reset`과 동일하게 RTC WDT를 이용해
 *   칩 전체를 리셋한다. (C3/C2/S2/S3 지원, 그 외는 USB-JTAG/클래식 폴백)
 */

const ESPRESSIF_USB_VID = 0x303a;
const RTC_CNTL_WDT_WKEY = 0x50d83aa1;

interface WdtRegisters {
  wdtConfig0: number;
  wdtConfig1: number;
  wdtWProtect: number;
}

/** 칩별 RTC WDT 레지스터 주소 (esptool targets/*.py 기준) */
const WDT_RESET_CHIPS: Record<string, WdtRegisters> = {
  "ESP32-C3": { wdtConfig0: 0x60008090, wdtConfig1: 0x60008094, wdtWProtect: 0x600080a8 },
  "ESP32-C2": { wdtConfig0: 0x60008084, wdtConfig1: 0x60008088, wdtWProtect: 0x6000809c },
  "ESP32-S2": { wdtConfig0: 0x3f408094, wdtConfig1: 0x3f408098, wdtWProtect: 0x3f4080ac },
  "ESP32-S3": { wdtConfig0: 0x60008098, wdtConfig1: 0x6000809c, wdtWProtect: 0x600080b0 },
};

const sleep = (ms: number): Promise<void> => new Promise((resolve) => setTimeout(resolve, ms));

/** RTC WDT로 칩 리셋. 지원하지 않는 칩이면 false. */
async function watchdogReset(loader: ESPLoader, transport: Transport): Promise<boolean> {
  const chipName = loader.chip?.CHIP_NAME;
  const registers = chipName ? WDT_RESET_CHIPS[chipName] : undefined;
  if (!registers) return false;

  // WDT가 발동하기 전에 부트스트랩 핀(EN/GPIO9)을 놓아 일반 부팅으로 진입시킨다.
  try {
    await transport.setDTR(false);
    await transport.setRTS(false);
  } catch {
    // 신호 제어 실패해도 WDT 리셋은 가능
  }

  // unlock → 타임아웃 설정 → enable/arm → re-lock
  // (1 << 31) 때문에 JS에서 음수가 되므로 >>> 0 으로 unsigned 처리
  try {
    await loader.writeReg(registers.wdtWProtect, RTC_CNTL_WDT_WKEY);
    await loader.writeReg(registers.wdtConfig1, 2000);
    await loader.writeReg(registers.wdtConfig0, ((1 << 31) | (5 << 28) | (1 << 8) | 2) >>> 0);
    await loader.writeReg(registers.wdtWProtect, 0);
  } catch {
    // 리셋이 먼저 발동하면 마지막 writeReg가 실패할 수 있음 — 정상 동작
  }

  await sleep(300);
  return true;
}

/** 클래식 UART 브리지(CP210x/CH34x)용 EN 펄스 리셋 */
async function classicHardReset(transport: Transport): Promise<void> {
  await transport.setDTR(false); // GPIO0 high → 플래시에서 부팅
  await transport.setRTS(true); // EN low (리셋 유지)
  await sleep(100);
  await transport.setRTS(false); // EN high → 앱 부팅
}

export type ResetMethod = "watchdog" | "usb-jtag" | "classic";

/**
 * 플래시된 펌웨어로 재부팅시킨다.
 * 1) RTC WDT 리셋 (C3/C2/S2/S3 — DeskStream ESP32-C3의 기본 경로)
 * 2) Espressif 내장 USB(Vendor 0x303A)면 USB-JTAG 리셋 시퀀스
 * 3) 그 외에는 클래식 EN 펄스
 */
export async function resetIntoApp(
  loader: ESPLoader,
  transport: Transport,
  port: SerialPort,
): Promise<ResetMethod> {
  if (await watchdogReset(loader, transport)) return "watchdog";

  if (port.getInfo().usbVendorId === ESPRESSIF_USB_VID) {
    await new UsbJtagSerialReset(transport).reset();
    return "usb-jtag";
  }

  await classicHardReset(transport);
  return "classic";
}
