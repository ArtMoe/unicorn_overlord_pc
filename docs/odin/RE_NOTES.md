# Odin Sphere Leifthrasir (CUSA05083 01.01) — RE notes

Addresses are PS4 vaddrs (image base 0), as in the Ghidra project.

## Frame limiter — FUN_006b3eb0 (end-of-frame / present)

Raw dump: `callers_vblank_flip.txt`.

- Only call site of `sceVideoOutWaitVblank` (call @ 0x6b409f, handle = [ctx + 0x12360]).
- After the vblank wait it spins with `scePthreadYield` until
  `sceKernelGetProcessTime() - last >= 1000000.0f / *(float *)(param_1 + 0x54)`
  (constant 1e6 at 0x86e2b0 → microseconds; **param_1+0x54 = target fps**).
  The spin is skipped when `*(int *)(param_1 + 0x24c) >= 2` or on the first frame.
- `DAT_00c614f0` = timestamp of the last frame (µs).
- `param_1 + 0x10` = frame counter (+1 per frame unless `param_1 + 0x240` is set).
- `param_1 + 0x38` = buffer index cycling 0,1,2 (triple buffering); per-object state is
  copied into one of 3 slots (stride 0x2d0) each frame, so render state is double/triple
  buffered and likely separate from simulation.
- `param_1 + 0x1f0` = delta of the counter at DAT_00c5b860/68 (wraps at DAT_00c5b858);
  candidate for the timestep fed to game logic — check before unlocking fps.

Next: find who writes param_1+0x54 (fps setting) and whether logic uses a fixed
step per frame (Vanillaware games usually do) or the 0x1f0 delta.

## Flip

- `sceGnmSubmitAndFlipCommandBuffers` / `...ForWorkload`: game wrappers at 0x682c00 /
  0x682c10 jump to the PLT stubs at 0x7d4e30 / 0x7d4e40.

## Render targets and resolution (see src/odin_render_scale.h)

- FUN_006ab500 sets up 3 "MAIN BUFFER[n]" + "DepthBuffer" pairs (1920x1080 immediates) and heaps;
  "RENDERBUFFER" heap 0x4000000 (imm at 0x6ab61e), "TEXTURE" heap 0x10000000.
- FUN_006adaa0(ctx, w, h): "USER OFFSCREEN BUFFER[n]" (4 slots, stride 0xe8, T# at +0x54); called
  3x at init (1920x1080) and from FUN_006b5900 (layer buffer, size from the screen getters
  FUN_006b3dd0/FUN_006b3de0 via FUN_006a90b0).
- Screen config `[0xc456a8]+0x40`: floats w/h at +0x3c/+0x40 (projection; left at 1920x1080).
- View: rect floats +0x284..+0x290, RT index +0x29c; FUN_006b5f80 sets its viewport through
  0x744910 (Gnmx setupScreenViewport).
- FUN_006b4140(ctx, cmd, x, y, w, h): clip scissor (FUN_006b34e0), tail-jumps to 0x6810c0.
- Texel coordinates are divided by T# sizes read on the CPU: sprite batches (FUN_006b6aa0 @
  0x6b87b1, constant to the sprite VS), layer composites (0x6b9165, 0x6ba126: 1/w, 1/h).
- FUN_006b9740: rectangle clear in 1080p units (VS divides by a constant 1920x1080).
