# Local cpal patches

This is cpal 0.15.3 from crates.io, vendored only because Namaz still uses
rodio 0.20, which depends on the cpal 0.15 API.  The patches are intentionally
small and limited to the ALSA backend.

## Why

In upstream 0.15.3, `process_output()` calls the error callback and then
`continue`s on every failed `writei()`.  A device that rejects writes
permanently (for example after a PipeWire restart, or an ALSA `file` PCM whose
target returns `ENOSPC`) makes the `cpal_alsa_out` thread spin forever at 100 %
CPU.  The rodio sink keeps consuming source samples before the failed write,
so a position-based watchdog does not see a stall.  `Stream::drop()` then
blocks in `join()` because the worker never observes the trigger pipe.

## Changes

- `process_output()` returns the write error instead of retrying in a tight
  loop.  A failed xrun recovery is returned the same way.
- The output worker backs off for 10 ms on an error, checking the stop trigger
  while it waits.  Errors that recur within one second accumulate, and the
  worker stops after three of them; this catches devices that occasionally
  accept a write between failures.  The stream can then be joined immediately
  and the next playback opens a fresh device.
- The read end of the trigger pipe is kept in `Stream` (as upstream cpal 0.18
  does), so `Stream::drop()` cannot panic if the worker exited early.
- Ignore an already-closed trigger pipe during `Drop`, and retry interrupts in
  `clear_pipe()`.

Everything else is unmodified upstream cpal 0.15.3.  Upstream licence:
Apache-2.0 (see `LICENSE`).
