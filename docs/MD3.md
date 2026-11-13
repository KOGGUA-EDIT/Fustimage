# Material Design 3 落地清单

种子色 `#6750A4`（tonal spot），亮/暗两套语义角色直接取自规范里导出的
`assets/tokens.css`，代码里对应 `src/theme.c` 的 `FusTheme`。

## 颜色角色（亮色 / 暗色）

| 角色 | 亮色 | 暗色 |
| --- | --- | --- |
| surface | `#FDF7FF` | `#141218` |
| surface-container-low | `#F8F2FA` | `#1D1B20` |
| surface-container | `#F2ECF4` | `#211F24` |
| surface-container-high | `#ECE6EE` | `#2B292F` |
| surface-container-highest | `#E6E0E9` | `#36343A` |
| on-surface | `#1D1B20` | `#E6E0E9` |
| on-surface-variant | `#49454E` | `#CAC4CF` |
| outline / outline-variant | `#7A757F` / `#CAC4CF` | `#948F99` / `#49454E` |
| primary / on-primary | `#65558F` / `#FFFFFF` | `#CFBDFE` / `#36275D` |
| primary-container | `#E9DDFF` | `#4D3D75` |
| secondary-container | `#E8DEF8` | `#4A4458` |
| tertiary / tertiary-container | `#7E5260` / `#FFD9E3` | `#EFB8C8` / `#633B48` |
| error | `#BA1A1A` | `#FFB4AB` |
| inverse-surface / inverse-on-surface | `#322F35` / `#F5EFF7` | `#E6E0E9` / `#322F35` |

阴影只在按键抬起的浮动层（snackbar、sheet）上出现，层级主要靠
surface-container 的四级色阶区分，符合「不靠阴影表达高度」的要求。

## 排版

固定 15 级 type scale 里，界面只用到 9 级；CJK 场景把 letter-spacing 归零、
行高比拉丁规范多 2–4 px。

| 用途 | 规格 |
| --- | --- |
| headline-small | 24/32 · 400 |
| title-large | 22/28 · 400 |
| title-medium | 16/24 · 500 |
| body-large | 16/24 · 400 |
| body-medium | 14/20 · 400 |
| body-small | 12/16 · 400 |
| label-large | 14/20 · 500 |
| label-medium | 12/16 · 500 |
| label-small | 11/16 · 500 |

字体链：`Segoe UI Variable Display` → `Segoe UI`，中文回退 `Microsoft YaHei UI` /
`Noto Sans SC`。字形不合成粗体、不用斜体。

## 形状与状态

- 形状档位：0 / 4 / 8 / 12 / 16 / 28 / 全圆角。按钮与分段控件用全圆角（20 dp），
  图标块 12 dp，chip 8 dp，snackbar 6 dp。
- state layer：hover 8%、pressed 10%、拖拽 16%，画在控件自身的圆角上。
- 动效：`cubic-bezier(.2, 0, 0, 1)`；sheet 滑入 200 ms，图片淡入 220 ms，
  snackbar 4 s 过期（进入/退出各 120 ms）。
- 触控/点击目标不小于 48 dp（图标块 48、chip 32 但外扩点击区、FAB 56）。

## 组件与结构

- **单目的地**：一个画布 + 一个 supporting pane，没有导航组件（不需要 rail / bar）。
- **top app bar** 64 dp：品牌方块 28 dp + 标题（title-large）；右侧依次是
  撤销 / 重做 / 深浅色（弯月 / 太阳）与一个 filled 的保存图标按钮。
- **pane**（≥840 dp 常驻 340/360 dp；<840 dp 收成 tool sheet）：
  - 唯一的 filled 主操作是「打开图片」的 56 dp 圆形按钮；
  - 旋转 4 个图标块、裁剪比例 chip 行、翻转/滤镜 4 个图标块、
    「适应 / 实际大小」分段按钮；
  - 全部为图标，文字只在悬停提示里出现。
- **裁剪模式**：沿用第一版的交互 —— 应用（✓，filled）/ 取消（✕，tonal）
  放在面板顶部，画布上给出三分线、四角手柄和 45% 遮罩；`Enter` / `Esc` 等价。
- **空状态**：96 dp tonal 圆盘 + 图标，配一行标题与一行操作提示。

## 无障碍与响应式

- 每个图标按钮都带工具提示与快捷键提示；键盘可完成打开、保存、撤销、
  旋转、翻转、滤镜、缩放、裁剪、主题切换。
- 三档尺寸类：<600 dp（紧凑，工具进 sheet）、600–839 dp（侧栏 340 dp）、
  ≥840 dp（侧栏 340/360 dp）；窗口最小 640×480 dp。
- 深浅色切换同步 DWM 标题栏，偏好写入 `HKCU\Software\Fustimage`。

## 自检对照

| 规范项 | 落地 |
| --- | --- |
| 固定 type scale、权重只用 400/500 | `theme.c` 9 级格式，全部 400/500 |
| 每组件各自的圆角 | 按钮 20、图标块 12、chip 8、snackbar 6 |
| 不靠阴影表达层级 | 只用 surface-container 色阶；阴影仅浮层 |
| state layer 8/10/16% | `state_layer()` 统一实现 |
| 48 dp 目标 | 图标块 48、FAB 56、chip 外扩 |
| 一屏一个 filled 主操作 | 打开图片（裁剪时换成「应用」） |
| 90% 中性色 | 只有主操作、选中态与品牌块用 primary 系 |
| 动效梯度 | 200–220 ms，`cubic-bezier(.2,0,0,1)` |
| 尺寸类切换导航 | <840 dp 面板收成 sheet |
