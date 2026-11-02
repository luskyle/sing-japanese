import { afterEach, describe, expect, it, vi } from "vitest";
import { PronunciationPlayer } from "./audio";

class FakeAudio {
  static instances: FakeAudio[] = [];
  playbackRate = 1;
  preservesPitch = true;
  src = "";
  onended: (() => void) | null = null;
  onerror: (() => void) | null = null;
  error: { code: number } | null = null;
  pause = vi.fn();
  load = vi.fn();
  removeAttribute = vi.fn();
  play = vi.fn().mockResolvedValue(undefined);
  constructor() { FakeAudio.instances.push(this); }
}

function setup() {
  FakeAudio.instances = [];
  vi.stubGlobal("Audio", FakeAudio);
  const fetch = vi.fn().mockResolvedValue(new Response(new Uint8Array([79, 103, 103, 83])));
  vi.stubGlobal("fetch", fetch);
  const create = vi.spyOn(URL, "createObjectURL").mockReturnValue("blob:local-audio");
  const revoke = vi.spyOn(URL, "revokeObjectURL").mockImplementation(() => {});
  return { fetch, create, revoke };
}

afterEach(() => {
  vi.restoreAllMocks();
  vi.unstubAllGlobals();
});

describe("offline pronunciation player", () => {
  it("plays custom-protocol assets through a Blob and releases them on stop", async () => {
    const { fetch, create, revoke } = setup();
    const player = new PronunciationPlayer();
    const ended = vi.fn();
    const failed = vi.fn();
    expect(await player.play("a", 0.8, ended, failed)).toBe(true);
    expect(fetch).toHaveBeenCalledWith("/audio/a.ogg", expect.objectContaining({ signal: expect.any(AbortSignal) }));
    expect(create).toHaveBeenCalledWith(expect.any(Blob));
    const audio = FakeAudio.instances[0];
    expect(audio.src).toBe("blob:local-audio");
    expect(audio.playbackRate).toBe(0.8);
    expect(audio.preservesPitch).toBe(true);
    audio.onended?.();
    expect(ended).toHaveBeenCalledOnce();
    player.stop();
    expect(revoke).toHaveBeenCalledWith("blob:local-audio");
    expect(audio.pause).toHaveBeenCalled();
    expect(failed).not.toHaveBeenCalled();
  });

  it("ignores an obsolete fetch when selection changes before loading finishes", async () => {
    const { fetch, create } = setup();
    let resolveResponse: (response: Response) => void = () => { throw new Error("Missing deferred request"); };
    fetch.mockReturnValueOnce(new Promise<Response>((resolve) => { resolveResponse = resolve; }));
    const player = new PronunciationPlayer();
    const failed = vi.fn();
    const started = player.play("a", 1, vi.fn(), failed);
    player.stop();
    resolveResponse(new Response(new Uint8Array([79, 103, 103, 83])));
    expect(await started).toBe(false);
    expect(create).not.toHaveBeenCalled();
    expect(failed).not.toHaveBeenCalled();
    expect(FakeAudio.instances[0].play).not.toHaveBeenCalled();
  });

  it("reports missing resources explicitly without pretending playback succeeded", async () => {
    const { fetch } = setup();
    fetch.mockResolvedValueOnce(new Response("", { status: 404 }));
    const player = new PronunciationPlayer();
    const failed = vi.fn();
    const ended = vi.fn();
    expect(await player.play("a", 1, ended, failed)).toBe(false);
    expect(failed).toHaveBeenCalledWith(expect.stringContaining("HTTP 404"));
    expect(ended).not.toHaveBeenCalled();
  });

  it("explains an unavailable preview server as a resource loading failure, not an output problem", async () => {
    const { fetch } = setup();
    fetch.mockRejectedValueOnce(new TypeError("Failed to fetch"));
    const failed = vi.fn();
    expect(await new PronunciationPlayer().play("a", 1, vi.fn(), failed)).toBe(false);
    expect(failed).toHaveBeenCalledWith(expect.stringContaining("本地服务仍在运行"));
    expect(failed.mock.calls[0][0]).not.toContain("系统音频输出");
  });

  it("distinguishes playback permission errors from resource loading errors", async () => {
    setup();
    const player = new PronunciationPlayer();
    const failed = vi.fn();
    const started = player.play("a", 1, vi.fn(), failed);
    FakeAudio.instances[0].play.mockRejectedValueOnce(new DOMException("Permission denied", "NotAllowedError"));
    expect(await started).toBe(false);
    expect(failed).toHaveBeenCalledWith(expect.stringContaining("播放权限"));
    expect(failed.mock.calls[0][0]).not.toContain("本地服务");
  });
});
