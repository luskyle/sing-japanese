import { kana, type Kana, type Script } from "./kana";

export interface Card {
  listens: number;
  correct: number;
  wrong: number;
  streak: number;
}
export interface Progress {
  version: 1;
  cards: Record<string, Card>;
}
export interface Question {
  target: Kana;
  choices: Kana[];
}

export const emptyProgress = (): Progress => ({ version: 1, cards: {} });
export const cardKey = (script: Script, id: string): string => `${script}:${id}`;
export const blankCard = (): Card => ({ listens: 0, correct: 0, wrong: 0, streak: 0 });

export function parseProgress(value: unknown): Progress {
  if (!value || typeof value !== "object" || !("version" in value) || value.version !== 1
    || !("cards" in value) || !value.cards || typeof value.cards !== "object"
    || Array.isArray(value.cards) || Object.keys(value).some((key) => key !== "version" && key !== "cards")) {
    throw new Error("学习记录格式或版本不受支持，请先备份原记录。");
  }
  const cards: Record<string, Card> = {};
  const validKeys = new Set(kana.flatMap((item) => [cardKey("hiragana", item.id), cardKey("katakana", item.id)]));
  for (const [key, card] of Object.entries(value.cards)) {
    if (!validKeys.has(key) || !card || typeof card !== "object"
      || Object.keys(card).length !== 4
      || !["listens", "correct", "wrong", "streak"].every((field) =>
        field in card && Number.isInteger(card[field]) && card[field] >= 0 && card[field] <= 0xffffffff)
      || card.streak > card.correct) {
      throw new Error(`无效的学习记录：${key}`);
    }
    cards[key] = { listens: card.listens, correct: card.correct, wrong: card.wrong, streak: card.streak };
  }
  return { version: 1, cards };
}

export function record(
  progress: Progress, script: Script, id: string, event: "listen" | "correct" | "wrong",
): Progress {
  const key = cardKey(script, id);
  const card = { ...(progress.cards[key] ?? blankCard()) };
  const increment = (value: number): number => Math.min(0xffffffff, value + 1);
  if (event === "listen") card.listens = increment(card.listens);
  if (event === "correct") {
    card.correct = increment(card.correct);
    card.streak = increment(card.streak);
  }
  if (event === "wrong") {
    card.wrong = increment(card.wrong);
    card.streak = 0;
  }
  return { version: 1, cards: { ...progress.cards, [key]: card } };
}

export function summarize(progress: Progress, script: Script) {
  const cards = kana.map((item) => progress.cards[cardKey(script, item.id)] ?? blankCard());
  const correct = cards.reduce((sum, card) => sum + card.correct, 0);
  const wrong = cards.reduce((sum, card) => sum + card.wrong, 0);
  return {
    heard: cards.filter((card) => card.listens > 0).length,
    familiar: cards.filter((card) => card.streak >= 3).length,
    correct, wrong,
    accuracy: correct + wrong === 0 ? null : Math.round(correct / (correct + wrong) * 100),
  };
}

export function shuffle<T>(values: readonly T[], random: () => number = Math.random): T[] {
  const result = [...values];
  for (let index = result.length - 1; index > 0; index--) {
    const other = Math.floor(random() * (index + 1));
    [result[index], result[other]] = [result[other], result[index]];
  }
  return result;
}

export function makeQuestion(
  pool: Kana[], progress: Progress, script: Script,
  previousId?: string, random: () => number = Math.random,
): Question {
  if (new Set(pool.map((item) => item.sound)).size < 2) {
    throw new Error("听辨练习至少需要两个不同发音，请选择更多行。");
  }
  const candidates = pool.filter((item) => item.id !== previousId);
  const weights = candidates.map((item) => {
    const card = progress.cards[cardKey(script, item.id)] ?? blankCard();
    return 1 + (card.streak < 3 ? 3 : 0) + (card.wrong > card.correct ? 2 : 0);
  });
  let draw = random() * weights.reduce((sum, weight) => sum + weight, 0);
  let target = candidates[candidates.length - 1];
  for (let index = 0; index < candidates.length; index++) {
    draw -= weights[index];
    if (draw < 0) {
      target = candidates[index];
      break;
    }
  }
  const sounds = new Set([target.sound]);
  const distractors = shuffle(pool, random).filter((item) => {
    if (sounds.has(item.sound)) return false;
    sounds.add(item.sound);
    return true;
  }).slice(0, 3);
  return { target, choices: shuffle([target, ...distractors], random) };
}
