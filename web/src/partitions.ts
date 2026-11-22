/** 기기 플래시의 파티션 테이블 (ESP-IDF 형식: 0x8000, 32바이트 항목) */
export const PARTITION_TABLE_OFFSET = 0x8000;
export const PARTITION_TABLE_SIZE = 0xc00;

const ENTRY_SIZE = 32;
const ENTRY_MAGIC = 0x50aa;

export interface Partition {
  type: number;
  subtype: number;
  offset: number;
  size: number;
  label: string;
}

/** 런처 레이아웃에서 앱을 설치할 위치 */
export interface LauncherSlots {
  ota0: Partition;
  otadata: Partition;
}

export function parsePartitionTable(data: Uint8Array): Partition[] {
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const partitions: Partition[] = [];
  for (let pos = 0; pos + ENTRY_SIZE <= data.length; pos += ENTRY_SIZE) {
    if (view.getUint16(pos, true) !== ENTRY_MAGIC) break; // MD5 항목(0xEBEB) 또는 빈 영역(0xFFFF)에서 끝
    const labelBytes = data.subarray(pos + 12, pos + 28);
    const end = labelBytes.indexOf(0);
    partitions.push({
      type: data[pos + 2],
      subtype: data[pos + 3],
      offset: view.getUint32(pos + 4, true),
      size: view.getUint32(pos + 8, true),
      label: new TextDecoder().decode(end >= 0 ? labelBytes.subarray(0, end) : labelBytes),
    });
  }
  return partitions;
}

/** factory(런처)·ota_0·otadata 가 모두 있으면 런처가 설치된 기기로 본다. */
export function findLauncherSlots(partitions: Partition[]): LauncherSlots | null {
  const find = (type: number, subtype: number) =>
    partitions.find((p) => p.type === type && p.subtype === subtype);
  const factory = find(0x00, 0x00);
  const ota0 = find(0x00, 0x10);
  const otadata = find(0x01, 0x00);
  return factory && ota0 && otadata ? { ota0, otadata } : null;
}
