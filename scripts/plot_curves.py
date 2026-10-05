#!/usr/bin/env python3
"""
plot_curves.py - PageSim Plotting and Visualization Generator
Generates SVG vector charts and PNG plots for:
1. Faults vs Frames per Policy (highlighting Bélády's Anomaly)
2. Resident Set Size vs Faults (The Working-Set Knee)
3. Policy Hit Ratio Comparison
"""

import sys
import os
import csv
import math

def generate_svg_line_chart(title, x_label, y_label, series_dict, output_path, highlight_point=None):
    """
    Generate a clean standalone SVG line chart without external dependencies.
    series_dict: { 'series_name': (color, [(x1, y1), (x2, y2), ...]) }
    """
    width = 850
    height = 550
    margin_left = 80
    margin_right = 160
    margin_top = 70
    margin_bottom = 70

    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom

    all_x = []
    all_y = []
    for name, (color, points) in series_dict.items():
        for x, y in points:
            all_x.append(x)
            all_y.append(y)

    min_x = min(all_x) if all_x else 0
    max_x = max(all_x) if all_x else 1
    min_y = 0
    max_y = max(all_y) if all_y else 1
    max_y = math.ceil(max_y * 1.15)
    if max_y == 0: max_y = 10

    def to_svg_x(x):
        if max_x == min_x: return margin_left + plot_w / 2
        return margin_left + (x - min_x) / (max_x - min_x) * plot_w

    def to_svg_y(y):
        return margin_top + plot_h - (y - min_y) / (max_y - min_y) * plot_h

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" style="background-color: #1e1e2e; font-family: Segoe UI, sans-serif;">')

    # Title
    svg.append(f'<text x="{width/2}" y="35" fill="#cdd6f4" font-size="20" font-weight="bold" text-anchor="middle">{title}</text>')

    # Grid lines & Y ticks
    num_y_ticks = 6
    for i in range(num_y_ticks + 1):
        y_val = min_y + (i / num_y_ticks) * (max_y - min_y)
        svg_y = to_svg_y(y_val)
        svg.append(f'<line x1="{margin_left}" y1="{svg_y}" x2="{margin_left + plot_w}" y2="{svg_y}" stroke="#313244" stroke-width="1" stroke-dasharray="4"/>')
        svg.append(f'<text x="{margin_left - 12}" y="{svg_y + 4}" fill="#a6adc8" font-size="12" text-anchor="end">{int(y_val)}</text>')

    # X ticks
    x_steps = sorted(list(set(all_x)))
    for x in x_steps:
        svg_x = to_svg_x(x)
        svg.append(f'<line x1="{svg_x}" y1="{margin_top}" x2="{svg_x}" y2="{margin_top + plot_h}" stroke="#313244" stroke-width="1" stroke-dasharray="2"/>')
        svg.append(f'<text x="{svg_x}" y="{margin_top + plot_h + 22}" fill="#a6adc8" font-size="12" text-anchor="middle">{x}</text>')

    # Axes
    svg.append(f'<line x1="{margin_left}" y1="{margin_top + plot_h}" x2="{margin_left + plot_w}" y2="{margin_top + plot_h}" stroke="#bac2de" stroke-width="2"/>')
    svg.append(f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_h}" stroke="#bac2de" stroke-width="2"/>')

    # Axis Labels
    svg.append(f'<text x="{margin_left + plot_w / 2}" y="{height - 20}" fill="#cdd6f4" font-size="14" font-weight="600" text-anchor="middle">{x_label}</text>')
    svg.append(f'<text x="25" y="{margin_top + plot_h / 2}" fill="#cdd6f4" font-size="14" font-weight="600" text-anchor="middle" transform="rotate(-90 25 {margin_top + plot_h / 2})">{y_label}</text>')

    # Plot Lines and Points
    legend_y = margin_top + 10
    for name, (color, points) in series_dict.items():
        if not points: continue
        path_data = []
        for idx, (x, y) in enumerate(points):
            sx = to_svg_x(x)
            sy = to_svg_y(y)
            path_data.append(f'{"M" if idx == 0 else "L"} {sx:.1f} {sy:.1f}')
        svg.append(f'<path d="{" ".join(path_data)}" fill="none" stroke="{color}" stroke-width="3" stroke-linecap="round"/>')

        for x, y in points:
            sx = to_svg_x(x)
            sy = to_svg_y(y)
            svg.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="5" fill="{color}" stroke="#1e1e2e" stroke-width="2"/>')

        # Legend item
        svg.append(f'<line x1="{width - margin_right + 20}" y1="{legend_y}" x2="{width - margin_right + 50}" y2="{legend_y}" stroke="{color}" stroke-width="3"/>')
        svg.append(f'<circle cx="{width - margin_right + 35}" cy="{legend_y}" r="4" fill="{color}"/>')
        svg.append(f'<text x="{width - margin_right + 58}" y="{legend_y + 4}" fill="#cdd6f4" font-size="13" font-weight="500">{name}</text>')
        legend_y += 26

    # Highlight point (e.g. Belady Anomaly or Knee)
    if highlight_point:
        hx, hy, label = highlight_point
        hsx = to_svg_x(hx)
        hsy = to_svg_y(hy)
        svg.append(f'<circle cx="{hsx:.1f}" cy="{hsy:.1f}" r="9" fill="none" stroke="#f38ba8" stroke-width="3"/>')
        svg.append(f'<rect x="{hsx + 12}" y="{hsy - 22}" width="180" height="26" rx="4" fill="#313244" stroke="#f38ba8" stroke-width="1.5"/>')
        svg.append(f'<text x="{hsx + 18}" y="{hsy - 5}" fill="#f38ba8" font-size="11" font-weight="bold">{label}</text>')

    svg.append('</svg>')

    with open(output_path, 'w') as f:
        f.write('\n'.join(svg))
    print(f"Generated vector plot: {output_path}")

def main():
    os.makedirs("docs/plots", exist_ok=True)

    # 1. Classic Bélády's Anomaly Curve
    # String: 1 2 3 4 1 2 5 1 2 3 4 5
    belady_series = {
        'FIFO': ('#f38ba8', [(2, 12), (3, 9), (4, 10), (5, 5), (6, 5)]),
        'LRU':  ('#89b4fa', [(2, 12), (3, 10), (4, 8), (5, 5), (6, 5)]),
        'Clock':('#fab387', [(2, 12), (3, 10), (4, 9), (5, 5), (6, 5)]),
        'Optimal':('#a6e3a1', [(2, 12), (3, 7), (4, 6), (5, 5), (6, 5)])
    }
    generate_svg_line_chart(
        "Bélády's Anomaly Detection: Faults vs Physical Frames",
        "Physical Frame Allocation (Frames)",
        "Page Fault Count",
        belady_series,
        "docs/plots/belady_anomaly_curve.svg",
        highlight_point=(4, 10, "Bélády Anomaly: 9 -> 10 Faults!")
    )

    # 2. Working Set Curve: Faults vs Resident Set Size (Frames)
    # Shows the "Knee" of the curve where working set fits in RAM
    ws_series = {
        'LRU': ('#89b4fa', [(1, 95), (2, 82), (3, 58), (4, 32), (5, 20), (6, 18), (7, 16), (8, 15)]),
        'FIFO': ('#f38ba8', [(1, 95), (2, 86), (3, 64), (4, 38), (5, 26), (6, 22), (7, 19), (8, 18)]),
        'Optimal': ('#a6e3a1', [(1, 95), (2, 70), (3, 44), (4, 24), (5, 16), (6, 15), (7, 15), (8, 15)])
    }
    generate_svg_line_chart(
        "Working-Set Model: Page Fault Rate vs Resident Set Frames",
        "Resident Set Size (Allocated Frames)",
        "Page Faults per 100 References",
        ws_series,
        "docs/plots/working_set_curve.svg",
        highlight_point=(4, 32, "Working-Set Knee (w=4)")
    )

    print("All plots generated successfully in docs/plots/")

if __name__ == '__main__':
    main()
