#!/usr/bin/env python3
"""Regenerates assets/diagrams/packet_layout.svg. Offsets mirror include/fwupdate/protocol.hpp.

Usage (from the repository root): python3 docs/diagrams/packet_layout.py
"""
FIELDS = [  # name, subtitle, offset, size (None = variable), colour
    ("MAGIC", '"FWUP"', 0, 4, "#1F6FEB"), ("VER", "0x01", 4, 1, "#0E7C86"),
    ("TYPE", "1-4", 5, 1, "#0E7C86"), ("RESERVED", "0x0000", 6, 2, "#5A6B85"),
    ("SEQUENCE", "u32 BE", 8, 4, "#1F6FEB"), ("LENGTH N", "u16 BE", 12, 2, "#0E7C86"),
    ("PAYLOAD", "0..256 bytes", 14, None, "#2E8B57"), ("CRC-32", "u32 BE", None, 4, "#B7791F"),
]
UNIT, X0, Y0, H = 46, 30, 70, 64
WIDTH = {4: 4 * UNIT, 1: 1.4 * UNIT, 2: 2 * UNIT, None: 4.2 * UNIT}


def main() -> None:
    boxes, x = [], X0
    for name, sub, off, size, col in FIELDS:
        w = WIDTH[size]
        boxes.append((name, sub, off, size, col, x, w))
        x += w
    total = x + X0
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{total:.0f}" height="250" viewBox="0 0 {total:.0f} 250" font-family="Segoe UI, Helvetica, Arial, sans-serif">',
           f'<rect width="{total:.0f}" height="250" rx="12" fill="#0B1628"/>',
           '<text x="30" y="38" fill="#E6EEF8" font-size="18" font-weight="700">FWUP v1 packet layout</text>',
           '<text x="270" y="38" fill="#8FA7C4" font-size="13">big-endian · header 14 B · max packet 274 B</text>']
    for name, sub, off, size, col, bx, w in boxes:
        svg.append(f'<rect x="{bx + 1:.0f}" y="{Y0}" width="{w - 2:.0f}" height="{H}" rx="6" fill="{col}"/>')
        svg.append(f'<text x="{bx + w / 2:.0f}" y="{Y0 + 28}" fill="#FFFFFF" font-size="13" font-weight="700" text-anchor="middle">{name}</text>')
        svg.append(f'<text x="{bx + w / 2:.0f}" y="{Y0 + 48}" fill="#E6EEF8" font-size="11" text-anchor="middle">{sub}</text>')
        svg.append(f'<text x="{bx + 4:.0f}" y="{Y0 - 8}" fill="#8FA7C4" font-size="11">{off if off is not None else "14+N"}</text>')
        svg.append(f'<text x="{bx + w / 2:.0f}" y="{Y0 + H + 18}" fill="#8FA7C4" font-size="11" text-anchor="middle">{f"{size} B" if size else "N B"}</text>')
    crc_x = boxes[-1][5]
    svg.append(f'<path d="M{X0} {Y0 + H + 32} v8 H{crc_x - 4:.0f} v-8" fill="none" stroke="#B7791F" stroke-width="2"/>')
    svg.append(f'<text x="{(X0 + crc_x) / 2:.0f}" y="{Y0 + H + 60}" fill="#F2C46D" font-size="12" text-anchor="middle">CRC-32/ISO-HDLC covers bytes [0, 14+N) — header and payload, not the CRC field</text>')
    svg.append('<text x="30" y="232" fill="#8FA7C4" font-size="11">START payload: target_version u32 | image_length u32 | image_crc u32 (12 B) · DATA: 1..256 B · FINISH / ABORT: 0 B</text>')
    svg.append('</svg>')
    with open("assets/diagrams/packet_layout.svg", "w", encoding="utf-8") as fh:
        fh.write("\n".join(svg))


if __name__ == "__main__":
    main()
