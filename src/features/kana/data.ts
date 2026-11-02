export type Script = "hiragana" | "katakana";
export type Group = "basic" | "voiced" | "semi";
export interface Kana {
  id: string;
  hiragana: string;
  katakana: string;
  romaji: string;
  sound: string;
  group: Group;
  row: string;
  column: number;
  source: string;
}

const rows: {
  row: string;
  hira: string;
  kata: string;
  sounds: string[];
  group: Group;
  columns?: number[];
}[] = [
  { row: "元音", hira: "あいうえお", kata: "アイウエオ", sounds: ["a", "i", "u", "e", "o"], group: "basic" },
  { row: "K 行", hira: "かきくけこ", kata: "カキクケコ", sounds: ["ka", "ki", "ku", "ke", "ko"], group: "basic" },
  { row: "S 行", hira: "さしすせそ", kata: "サシスセソ", sounds: ["sa", "shi", "su", "se", "so"], group: "basic" },
  { row: "T 行", hira: "たちつてと", kata: "タチツテト", sounds: ["ta", "chi", "tsu", "te", "to"], group: "basic" },
  { row: "N 行", hira: "なにぬねの", kata: "ナニヌネノ", sounds: ["na", "ni", "nu", "ne", "no"], group: "basic" },
  { row: "H 行", hira: "はひふへほ", kata: "ハヒフヘホ", sounds: ["ha", "hi", "fu", "he", "ho"], group: "basic" },
  { row: "M 行", hira: "まみむめも", kata: "マミムメモ", sounds: ["ma", "mi", "mu", "me", "mo"], group: "basic" },
  { row: "Y 行", hira: "やゆよ", kata: "ヤユヨ", sounds: ["ya", "yu", "yo"], group: "basic", columns: [0, 2, 4] },
  { row: "R 行", hira: "らりるれろ", kata: "ラリルレロ", sounds: ["ra", "ri", "ru", "re", "ro"], group: "basic" },
  { row: "W 行", hira: "わを", kata: "ワヲ", sounds: ["wa", "wo"], group: "basic", columns: [0, 4] },
  { row: "拨音", hira: "ん", kata: "ン", sounds: ["n"], group: "basic" },
  { row: "G 行", hira: "がぎぐげご", kata: "ガギグゲゴ", sounds: ["ga", "gi", "gu", "ge", "go"], group: "voiced" },
  { row: "Z 行", hira: "ざじずぜぞ", kata: "ザジズゼゾ", sounds: ["za", "ji", "zu", "ze", "zo"], group: "voiced" },
  { row: "D 行", hira: "だぢづでど", kata: "ダヂヅデド", sounds: ["da", "di", "du", "de", "do"], group: "voiced" },
  { row: "B 行", hira: "ばびぶべぼ", kata: "バビブベボ", sounds: ["ba", "bi", "bu", "be", "bo"], group: "voiced" },
  { row: "P 行", hira: "ぱぴぷぺぽ", kata: "パピプペポ", sounds: ["pa", "pi", "pu", "pe", "po"], group: "semi" },
];

const sources: Record<string, string> = {
  a: "Ja-A.oga", i: "Japanese I.ogg", u: "Japanese U.ogg",
  e: "Ja-E.oga", o: "Japanese O.ogg", so: "Ja-So.oga",
  chi: "Japanese ti.ogg", tsu: "Japanese tsu.ogg",
  fu: "Japanese hu.ogg", ji: "Japanese zi.ogg", n: "Japanese N.ogg",
};

export const kana: Kana[] = rows.flatMap((row) =>
  row.sounds.map((id, index) => ({
    id,
    hiragana: [...row.hira][index],
    katakana: [...row.kata][index],
    romaji: id === "di" ? "ji" : id === "du" ? "zu" : id,
    sound: id === "wo" ? "o" : id === "di" ? "ji" : id === "du" ? "zu" : id,
    group: row.group,
    row: row.row,
    column: row.columns?.[index] ?? index,
    source: sources[id] ?? `Japanese ${id}.ogg`,
  })),
);

export const groupNames: Record<Group, string> = {
  basic: "清音 · 46",
  voiced: "浊音 · 20",
  semi: "半浊音 · 5",
};

export function pronunciationNote(item: Kana): string {
  switch (item.id) {
    case "wo": return "を／ヲ：现代标准语通常读作 o，主要用作助词；wo 是区分拼写的转写。";
    case "di": return "ぢ／ヂ：现代标准语通常与 じ／ジ 同音，不能仅凭听音区分字形。";
    case "du": return "づ／ヅ：现代标准语通常与 ず／ズ 同音，不能仅凭听音区分字形。";
    case "n": return "ん／ン 是一个音拍；实际发音会受后续声音影响，不要总读成固定的中文“嗯”。";
    case "u": return "う／ウ 的嘴唇通常不明显前突，注意听原音，不要直接套用中文“乌”。";
    case "fu": return "ふ／フ 的辅音通常由双唇靠近发出，不等同于英语 f。";
    case "shi": return "し／シ 的罗马音写作 shi。罗马音只是辅助，优先模仿日语录音。";
    case "chi": return "ち／チ 的罗马音写作 chi，不是英语 ti。";
    case "tsu": return "つ／ツ 的罗马音写作 tsu，注意开头的辅音组合。";
    case "ha": return "は／ハ 单独读 ha；作为主题助词「は」时读 wa。";
    case "he": return "へ／ヘ 单独读 he；作为方向助词「へ」时读 e。";
    default: return item.id.startsWith("r")
      ? "ら行辅音通常是舌尖轻触上齿龈，不等同于英语 r 或 l。"
      : "先听一遍，再跟读一遍；然后隐藏罗马音，尝试直接从字形想起声音。";
  }
}
