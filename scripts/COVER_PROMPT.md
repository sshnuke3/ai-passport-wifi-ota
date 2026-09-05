# AI Passport Wi-Fi OTA Loader — Cover Image Prompt

> This is the **English default**. The Simplified Chinese version lives in
> [COVER_PROMPT.zh_CN.md](COVER_PROMPT.zh_CN.md).

The cover image is only needed if you go down path B (submitting to the play
community). This branch is a personal tool and does not use it, but the prompt
is kept here for reference.

## Supported generators

- Midjourney / DALL·E 3 / Jimeng / Stable Diffusion — universal English prompts
- Aspect ratio: 1:1 (cover card) or 16:9 (banner), matching the official community
- Style: keep the pixel-art / low-saturation tone of the official "answer book"
  and "pocket tomato clock" covers

## English prompt (recommended)

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

## Backup: minimal monochrome version

```
Black square background. Centered: a transparent card outline in
single-pixel stroke, glowing cyan. Three Wi-Fi arcs above it in
white. One green check mark inside a circle at the bottom-right
corner. No text. 512x512, monochrome except cyan and green.
```

## Styles to avoid

- Real photography (does not match the official pixel-art tone)
- Blue enterprise SaaS gradients (official play covers never do this)
- Covers with the words "OTA / Wi-Fi / update" (other community plays rarely put text on covers)

## After generating

Before uploading, it is recommended to:

1. Resize to within 1080×1080 (community docs require ≤10 MiB; 1K square is well under but stays sharp)
2. Use `file` to confirm the MIME type (PNG, JPEG, or WebP)
3. Run it once through the official Web Flasher and screenshot to verify legibility (white / grey / mobile backgrounds)
