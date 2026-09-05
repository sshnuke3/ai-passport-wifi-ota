# AI Passport Wi-Fi OTA 装载器 — 封面图提示词

> 本文件是[COVER_PROMPT.md](COVER_PROMPT.md)的简体中文版本。

封面图只在你**走 B（提交玩法社区）**时需要，本分支作为个人工具**用不到**。留档备用。

## 适用生成器

- Midjourney / DALL·E 3 / 即梦 / Stable Diffusion 通用英文 prompt
- 比例为官方社区常用的 1:1（封面卡片）或 16:9（横幅）
- 风格与官方"答案之书"、"口袋番茄钟"等封面保持像素风/低饱和度调性

## 英文 prompt（推荐）

```
Pixel-art style product cover, no text overlay.
A transparent rectangular "AI Passport" smart card lying on a dark
matte surface, viewed from a slight 3/4 angle. A glowing Wi-Fi ripple
emits from the top of the card, three soft concentric arcs in cyan
blue. A small green circular icon near the bottom of the card shows
a downward arrow inside a circle, suggesting a download. The card's
own screen is lit with a soft warm amber backlight. Background is a
deep slate gradient. Mood: technical, calm, slightly futuristic.
8-bit pixel aesthetic, low-poly edges, soft bloom. 1024x1024.
```

## 中文 prompt（即梦 / 通义）

```
像素风产品封面，无文字。一张透明外壳的"AI Passport"智能卡片斜放在哑光
深色桌面上，从 3/4 角度俯视。卡片顶部发出三圈柔和的青色 Wi-Fi 信号波纹，
向四周扩散。卡片底部有一个绿色圆形下载图标（箭头朝下嵌入圆内），暗示
"无线更新"。卡片自带屏幕发出暖琥珀色背光。背景是深板岩蓝渐变。
调性：技术、冷静、略带未来感。8-bit 像素艺术，低多边形轮廓，柔和辉光。
1024×1024。
```

## 备用：极简单色版（强抗审查 / 老设备 e-ink 风格）

```
Black square background. Centered: a transparent card outline in
single-pixel stroke, glowing cyan. Three Wi-Fi arcs above it in
white. One green check mark inside a circle at the bottom-right
corner. No text. 512x512, monochrome except cyan and green.
```

## 不推荐的风格

- 真实摄影风（和官方像素风调性不符）
- 蓝色企业 SaaS 渐变（官方玩法从不这样）
- 含 "OTA / Wi-Fi / 更新" 文字的封面（社区里其它玩法几乎不放字）

## 出图后

正式上传前建议：

1. 缩到 1080×1080 之内（社区文档要求 ≤10 MiB，1K 平方远低于上限但保持清晰）
2. 用 `file` 命令确认 MIME（PNG 或 JPEG 或 WebP 三选一）
3. 自己拿官方 Web Flasher 跑一遍截图验证辨识度（白底/灰底/手机端都要看）
