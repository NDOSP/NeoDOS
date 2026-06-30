# video.mod — IPC Interface

Registered as `video`. Get PID via `modman_lookup("video", &pid)`.

Messages are 64 bytes. First 8 = `cmd` (uint64_t). Color values are packed `0x00RRGGBB`.

---

### cmd = 1 — putChar(char, color, x, y)

| Offset | Type | Field |
|--------|------|-------|
| 0      | u64  | cmd = 1 |
| 8      | u8   | character |
| 12     | u32  | color |
| 16     | i32  | x |
| 20     | i32  | y |

Draws one character at (x,y) in given color. No background.

---

### cmd = 2 — putString(string, color, x, y)

| Offset | Type | Field |
|--------|------|-------|
| 0      | u64  | cmd = 2 |
| 8      | u32  | color |
| 12     | i32  | x |
| 16     | i32  | y |
| 20     | char[44] | null-terminated string |

Draws text at (x,y). Supports `\n`. No background.

---

### cmd = 3 — drawRectangle(color, h, w, x, y)

| Offset | Type | Field |
|--------|------|-------|
| 0      | u64  | cmd = 3 |
| 8      | u32  | color |
| 12     | i32  | height |
| 16     | i32  | width |
| 20     | i32  | x |
| 24     | i32  | y |

Filled rectangle.

---

### cmd = 4 — clearScreen()

| Offset | Type | Field |
|--------|------|-------|
| 0      | u64  | cmd = 4 |

Fills entire screen with pure black (0x000000).
