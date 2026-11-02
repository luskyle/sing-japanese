import { describe, expect, it } from "vitest";
import { kana, pronunciationNote } from "./data";
import { cardKey, emptyProgress, makeQuestion, parseProgress, record, summarize } from "./progress";

describe("kana curriculum", () => {
  it("contains 71 unique corresponding hiragana and katakana with correct group sizes", () => {
    expect(kana).toHaveLength(71);
    for (const field of ["id", "hiragana", "katakana"] as const) {
      expect(new Set(kana.map((item) => item[field])).size).toBe(71);
    }
    expect(kana.filter((item) => item.group === "basic")).toHaveLength(46);
    expect(kana.filter((item) => item.group === "voiced")).toHaveLength(20);
    expect(kana.filter((item) => item.group === "semi")).toHaveLength(5);
    for (const item of kana) {
      expect(item.katakana.codePointAt(0)! - item.hiragana.codePointAt(0)!).toBe(0x60);
      expect(pronunciationNote(item).length).toBeGreaterThan(10);
    }
  });
});

describe("local learning progress", () => {
  it("tracks script-specific attempts without mutating earlier snapshots", () => {
    const start = emptyProgress();
    let updated = record(start, "hiragana", "a", "listen");
    for (let i = 0; i < 3; i++) updated = record(updated, "hiragana", "a", "correct");
    expect(start.cards).toEqual({});
    expect(summarize(updated, "hiragana")).toEqual({ heard: 1, familiar: 1, correct: 3, wrong: 0, accuracy: 100 });
    expect(summarize(updated, "katakana").heard).toBe(0);
    updated = record(updated, "hiragana", "a", "wrong");
    expect(updated.cards[cardKey("hiragana", "a")].streak).toBe(0);
    expect(summarize(updated, "hiragana").accuracy).toBe(75);
    expect(summarize(start, "hiragana").accuracy).toBeNull();
  });

  it("validates persisted records and rejects corruption rather than resetting it", () => {
    const saved = record(emptyProgress(), "katakana", "wo", "correct");
    expect(parseProgress(JSON.parse(JSON.stringify(saved)))).toEqual(saved);
    for (const value of [
      null, [], {}, { version: 2, cards: {} }, { version: 1, cards: [] },
      { version: 1, cards: {}, unexpected: true },
      { version: 1, cards: { "hiragana:invalid": { listens: 0, correct: 0, wrong: 0, streak: 0 } } },
      { version: 1, cards: { "hiragana:a": { listens: -1, correct: 0, wrong: 0, streak: 0 } } },
      { version: 1, cards: { "hiragana:a": { listens: 0, correct: 0, wrong: 0, streak: 1 } } },
    ]) expect(() => parseProgress(value)).toThrow();
  });
});

describe("listening questions", () => {
  it("never pits homophones against each other, even across many generated questions", () => {
    for (let index = 0; index < 1000; index++) {
      const question = makeQuestion(kana, emptyProgress(), "hiragana");
      expect(question.choices).toHaveLength(4);
      expect(question.choices).toContain(question.target);
      expect(new Set(question.choices.map((item) => item.sound)).size).toBe(4);
    }
    for (const pair of [["o", "wo"], ["ji", "di"], ["zu", "du"]]) {
      expect(() => makeQuestion(kana.filter((item) => pair.includes(item.id)), emptyProgress(), "hiragana")).toThrow();
    }
  });

  it("stays within selected rows and avoids immediately repeating a target", () => {
    const pool = kana.filter((item) => item.row === "元音");
    let previous: string | undefined;
    for (let index = 0; index < 100; index++) {
      const question = makeQuestion(pool, emptyProgress(), "katakana", previous);
      expect(question.target.id).not.toBe(previous);
      expect(question.choices.every((item) => item.row === "元音")).toBe(true);
      previous = question.target.id;
    }
    expect(() => makeQuestion([], emptyProgress(), "hiragana")).toThrow();
    const two = makeQuestion(kana.slice(0, 2), emptyProgress(), "hiragana");
    expect(two.choices).toHaveLength(2);
  });

  it("prioritizes unfamiliar cards with deterministic weights", () => {
    let progress = emptyProgress();
    for (let index = 0; index < 3; index++) progress = record(progress, "hiragana", "a", "correct");
    const pool = kana.slice(0, 2);
    const targets = Array.from({ length: 100 }, (_, index) =>
      makeQuestion(pool, progress, "hiragana", undefined, () => index / 100).target.id);
    expect(targets.filter((id) => id === "i")).toHaveLength(80);
  });
});
