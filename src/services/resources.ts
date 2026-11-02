import { kana } from "../features/kana/data";

export interface Recording {
  id: string;
  hiragana: string;
  katakana: string;
  sourcePage: string;
  author: string;
  license: string;
  bytes: number;
}

export interface ResourceCatalog {
  count: number;
  bytes: number;
  date: string;
  recordings: Recording[];
}

function isRecording(value: unknown): value is Recording {
  return !!value && typeof value === "object"
    && ["id", "hiragana", "katakana", "sourcePage", "author", "license"].every((key) =>
      key in value && typeof Reflect.get(value, key) === "string")
    && "bytes" in value && typeof value.bytes === "number" && Number.isFinite(value.bytes) && value.bytes > 0;
}

export async function loadResources(): Promise<ResourceCatalog> {
  const response = await fetch("/audio-manifest.json");
  if (!response.ok) throw new Error(`资源清单读取失败：HTTP ${response.status}`);
  const manifest: unknown = await response.json();
  if (!manifest || typeof manifest !== "object" || !("version" in manifest) || manifest.version !== 1
    || !("recordings" in manifest) || !Array.isArray(manifest.recordings)
    || manifest.recordings.length !== kana.length || !manifest.recordings.every(isRecording)
    || !("retrievedAt" in manifest) || typeof manifest.retrievedAt !== "string"
    || Number.isNaN(Date.parse(manifest.retrievedAt))) {
    throw new Error("本地资源清单不完整，请执行 npm run resources:verify。");
  }
  return {
    count: manifest.recordings.length,
    bytes: manifest.recordings.reduce((sum, item) => sum + item.bytes, 0),
    date: manifest.retrievedAt.slice(0, 10),
    recordings: manifest.recordings,
  };
}

export async function loadDictionaryLicense(): Promise<string> {
  const response = await fetch("/licenses/IPADIC-NOTICE.txt");
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.text();
}
