#!/usr/bin/env python3
import argparse
import glob
import pathlib
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_FONT = pathlib.Path("/private/tmp/PingFang-0.ttf")
FONT_DIR = ROOT / "src" / "fonts"


def gb2312_level_1_chars():
    chars = []
    for row in range(16, 56):
        for col in range(1, 95):
            try:
                chars.append(bytes([row + 0xA0, col + 0xA0]).decode("gb2312"))
            except UnicodeDecodeError:
                pass
    return "".join(chars)


def unique_chars(value):
    return "".join(dict.fromkeys(value))


def find_converter():
    cached = sorted(
        glob.glob(str(pathlib.Path.home() / ".npm" / "_npx" / "*" / "node_modules" / ".bin" / "lv_font_conv")),
        key=lambda path: pathlib.Path(path).stat().st_mtime,
        reverse=True,
    )
    if cached:
        return cached[0]
    return "npx"


def run_converter(converter, font_path, size, bpp, symbols):
    output = FONT_DIR / f"lifetodo_pingfang_{size}.c"
    name = f"lifetodo_pingfang_{size}"
    if converter == "npx":
        command = ["npx", "--yes", "lv_font_conv"]
    else:
        command = [converter]
    command.extend(
        [
            "--font",
            str(font_path),
            "-r",
            "0x20-0x7E",
            "--symbols",
            symbols,
            "-o",
            str(output),
            "--format",
            "lvgl",
            "--bpp",
            str(bpp),
            "--size",
            str(size),
            "--lv-font-name",
            name,
            "--lv-include",
            "lvgl.h",
            "--force-fast-kern-format",
            "--no-kerning",
        ]
    )
    subprocess.run(command, cwd=ROOT, check=True)
    print(f"{output.relative_to(ROOT)}: {output.stat().st_size} bytes")


def main():
    parser = argparse.ArgumentParser(description="Generate LifeTodo LVGL PingFang SC fonts.")
    parser.add_argument("--font", default=str(DEFAULT_FONT), help="PingFang TTF path")
    parser.add_argument("--bpp", type=int, default=2, choices=[1, 2, 3, 4, 8])
    args = parser.parse_args()

    font_path = pathlib.Path(args.font)
    if not font_path.exists():
        raise SystemExit(f"Font not found: {font_path}")

    punctuation = (
        "，。！？：；、“”‘’（）《》【】…—·￥％＋－×÷＝～｜"
        "「」『』　"
    )
    ui_symbols = (
        "✓✔●○◯◉◎□■▲▼△▽←→↑↓↗↘"
        "℃年月日时分秒周星期家人事项任务设备联网同步亮度蓝牙屏幕完成未完成待办"
        "配网热点密码保存扫描连接失败成功今日明天昨天重复周期提醒成员全部"
    )
    symbols = unique_chars(gb2312_level_1_chars() + punctuation + ui_symbols)
    print(f"Common Chinese symbols: {len(symbols)}")

    FONT_DIR.mkdir(parents=True, exist_ok=True)
    converter = find_converter()
    print(f"Converter: {converter}")
    for size in (24, 28):
        run_converter(converter, font_path, size, args.bpp, symbols)


if __name__ == "__main__":
    main()
