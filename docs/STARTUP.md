# 启动与载入测量

都是在同一台机器（Alienware m15 R6 / RTX 3050 Ti Laptop + Intel UHD，Console 会话）
上用 `bench\startup_bench.exe` 与程序内的 `FUST_TIMING` 打点测的。

## 怎么测

```powershell
# 黑盒：CreateProcess → 轮询找到该 PID 的可见顶层窗口 → WaitForInputIdle
.\bench\startup_bench.exe .\build\Fustimage.exe 7
# 输出列：iter,visible_ms,idle_ms,total_ms

# 白盒：程序自己往文件里追加时间戳（毫秒，相对进程入口）
$env:FUST_TIMING = ".\bench\timing.txt"
.\build\Fustimage.exe .\bench\big6k.jpg
```

打点位置：`entry / factories / window / shown / target / paint / loop /
load_start / load_done / upgrade_done`。

## 结果（安静时段，取中位数）

| 程序 | 窗口可见 | 首帧完成 |
| --- | --- | --- |
| `LightPic.exe`（GDI+ 立即模式） | 57–87 ms | 57–87 ms |
| `bench\null.exe`（空 Win32 窗口基线） | 27–63 ms | 143–190 ms |
| **新版 Fustimage** | **31 ms** | **94 ms** |
| 旧版 Fustimage（D3D 硬件目标） | 277–433 ms | 436–594 ms |

同样条件下再测一轮（同一分钟，桌面有 DSH / 浏览器常驻）：

| 程序 | 窗口可见 | 首帧完成 |
| --- | --- | --- |
| Fustimage（图标版，机器空闲） | 41–53 ms（中位 46） | 142–185 ms（中位 156） |
| Fustimage（图标版，桌面有其他程序在跑） | 65–102 ms（中位 94） | 134–197 ms（中位 179） |
| `bench\null.exe` | 29–35 ms | 114–174 ms |

也就是说窗口出现只比空窗口基线多 10–50 ms，差出来的部分就是软渲染目标与首帧绘制。

白盒单次（6000×4000 JPEG，6.6 MB）：

```
entry 0 · factories 16 · window 31 · shown 31 · target 78 · paint 94
loop 94 · load_start 94 · load_done 234 · upgrade_done 406
```

解读：

- 窗口在 **31 ms** 出现，第一帧内容在 **94 ms** 画完；`load_done` 说明 1600 px 预览已经解出来，
  `upgrade_done` 是后台把全尺寸图换上去的时刻 —— 这两步都不挡界面。
- 机器忙的时候（DSH、浏览器占 25–30% CPU）这些数字会整体放大 2–4 倍，
  `null.exe` 基线也同样变慢，所以对比时要同时看基线。

## 为什么旧版慢

1. `CreateHwndRenderTarget`（硬件 D2D）在混合显卡机器上等于一次 D3D11 设备初始化：
   `D3D11CreateDevice(HARDWARE)` 单次 248–298 ms，同进程第二个设备只要 19.7 ms
   ——这是驱动的一次性开销，换 D3D11+swapchain 的现代路径也一样（探针 `bench\probe3.exe`）。
2. 旧版在 `ShowWindow` 之前就建好设备、还要同步 `UpdateWindow` 画完第一帧，
   于是「能看到窗口」被推到 277–433 ms。
3. 首帧里画布铺满 24 px 步长的点阵（约 1900 个椭圆），单独值 15–50 ms；
   `EndDraw` 首次呈现 58–72 ms。

## 新版做了什么

- **WARP 软件渲染目标**（`D2D1_RENDER_TARGET_TYPE_SOFTWARE`）：建目标 30–55 ms，
  不再触发显卡驱动初始化；UI 是 2D 图形，软光栅完全够用。
- **先显窗、后建目标**：`CreateWindowExW`（隐藏）→ `ShowWindow` → 建目标 → 首帧，
  窗口在 31 ms 出现，内容随后补上。
- **启动路径只做必要的事**：不 `CoInitializeEx`、不碰 WIC、不建文件对话框、不做图标位图。
- **载入完全异步**：预览按 1600 px 解码先上屏，后台再用 8192 px 上限解码替换；
  两者都在工作线程，用 `load_gen` 丢弃过期结果。
- **按需重建**：DPR 变化才重建目标；点阵/棋盘格这类纹理缓存成位图刷子。

复现脚本与各阶段原始数据分别在 `bench\startup_bench.exe`、`bench\timing-*.txt`。

## 本轮复测（最终二进制，同机同会话）

`build\Fustimage.exe bench\big6k.jpg`，`FUST_TIMING=…` 白盒打点：

| 打点 | ms |
| --- | --- |
| entry / factories | 0 / 0 |
| window（CreateWindowExW 返回） | 31 |
| shown（ShowWindow 返回） | 93 |
| target（软渲染目标就绪） | 140 |
| paint（首帧画完） | 172 |
| load_start | 172 |
| load_done（1600 px 预览解码完成） | 359 |
| upgrade_done（6000×4000 全尺寸替换） | 562 |

即：**窗口 31 ms、首内容 172 ms**，预览 187 ms 后出现在画布上，全尺寸 390 ms 时替换完毕，全程主线程只做绘制。

## 测试钩子

- `FUST_TIMING=<文件>`：写出上面这些打点。
- `FUST_DEBUG_TITLE=1`：窗口标题显示 `fit/zoom/crop/pre/cr/pan/cw/drag/rot` 状态。
- `FUST_AUTOSAVE=<路径>`：加载完成后自动走一次导出（和「保存」按钮同一条 `start_export` 路径，只跳过文件对话框），用于脚本化验证。