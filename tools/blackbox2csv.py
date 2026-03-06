#!/usr/bin/env python3
"""Betaflight Blackbox CSV to ExpressLRS Sender CSV converter.

Betaflight Blackbox Explorerからエクスポートした CSV を、
expresslrs_sender で再生可能な CSV 形式に変換する。

Usage:
    python3 tools/blackbox2csv.py data/blackbox/BTFL_*.csv -o data/output.csv
"""

import argparse
import csv
import sys
from pathlib import Path


# CRSF channel value constants
CRSF_MIN = 172
CRSF_MID = 992
CRSF_MAX = 1811

# PWM range for CRSF conversion
PWM_MIN = 988
PWM_MAX = 2012

# Output settings
OUTPUT_INTERVAL_MS = 2  # 500Hz
DISARM_PAD_MS = 3000    # 3 seconds of disarmed padding


def pwm_to_crsf(pwm: float) -> int:
    """Convert PWM value (988-2012) to CRSF value (172-1811)."""
    crsf = (pwm - PWM_MIN) * (CRSF_MAX - CRSF_MIN) / (PWM_MAX - PWM_MIN) + CRSF_MIN
    return max(CRSF_MIN, min(CRSF_MAX, round(crsf)))


def rc_command_to_crsf(rc_cmd: float) -> int:
    """Convert rcCommand stick value (center=0) to CRSF value."""
    pwm = rc_cmd + 1500.0
    return pwm_to_crsf(pwm)


def throttle_to_crsf(throttle: float) -> int:
    """Convert rcCommand[3] throttle value (1000-2000) to CRSF value."""
    return pwm_to_crsf(throttle)


def find_header_row(filepath: Path) -> int:
    """Find the row number of the data header in the blackbox CSV."""
    with open(filepath, "r") as f:
        for i, line in enumerate(f):
            if '"loopIteration"' in line or "loopIteration" in line:
                return i
    raise ValueError("Could not find data header row (loopIteration) in the file")


def parse_blackbox_csv(filepath: Path) -> list[dict]:
    """Parse Betaflight blackbox CSV and extract RC command data."""
    header_row = find_header_row(filepath)

    frames = []
    with open(filepath, "r") as f:
        # Skip metadata rows before header
        for _ in range(header_row):
            next(f)

        reader = csv.DictReader(f, quoting=csv.QUOTE_ALL)

        for row in reader:
            # Skip rows with missing or invalid data
            try:
                time_us = int(row["time"])
                rc0 = float(row["rcCommand[0]"])
                rc1 = float(row["rcCommand[1]"])
                rc2 = float(row["rcCommand[2]"])
                rc3 = float(row["rcCommand[3]"])
            except (ValueError, KeyError):
                continue

            frames.append({
                "time_us": time_us,
                "roll": rc0,
                "pitch": rc1,
                "yaw": rc2,
                "throttle": rc3,
            })

    if not frames:
        raise ValueError("No valid data frames found in the blackbox CSV")

    return frames


def downsample(frames: list[dict], interval_ms: int) -> list[dict]:
    """Downsample frames to the target interval using nearest-neighbor."""
    if not frames:
        return []

    t_start_us = frames[0]["time_us"]
    t_end_us = frames[-1]["time_us"]
    duration_ms = (t_end_us - t_start_us) / 1000.0

    output = []
    src_idx = 0

    t_ms = 0.0
    while t_ms <= duration_ms:
        target_us = t_start_us + t_ms * 1000.0

        # Advance source index to nearest frame
        while src_idx + 1 < len(frames) and frames[src_idx + 1]["time_us"] <= target_us:
            src_idx += 1

        # Pick the closest frame
        if src_idx + 1 < len(frames):
            d0 = abs(frames[src_idx]["time_us"] - target_us)
            d1 = abs(frames[src_idx + 1]["time_us"] - target_us)
            frame = frames[src_idx] if d0 <= d1 else frames[src_idx + 1]
        else:
            frame = frames[src_idx]

        output.append({
            "time_ms": round(t_ms),
            "roll": frame["roll"],
            "pitch": frame["pitch"],
            "yaw": frame["yaw"],
            "throttle": frame["throttle"],
        })

        t_ms += interval_ms

    return output


def make_disarm_frame(time_ms: int) -> list[int]:
    """Create a disarmed frame with safe values."""
    # CH1=Roll(center), CH2=Pitch(center), CH3=Throttle(min),
    # CH4=Yaw(center), CH5=Arm(low=disarm), CH6-CH16=center/low
    return [
        time_ms,
        CRSF_MID,   # CH1 Roll
        CRSF_MID,   # CH2 Pitch
        CRSF_MIN,   # CH3 Throttle
        CRSF_MID,   # CH4 Yaw
        CRSF_MIN,   # CH5 Arm (disarmed)
        CRSF_MIN,   # CH6
        CRSF_MIN,   # CH7
        CRSF_MIN,   # CH8
        CRSF_MID,   # CH9
        CRSF_MID,   # CH10
        CRSF_MID,   # CH11
        CRSF_MID,   # CH12
        CRSF_MID,   # CH13
        CRSF_MID,   # CH14
        CRSF_MID,   # CH15
        CRSF_MID,   # CH16
    ]


def make_arm_frame(time_ms: int, roll: int, pitch: int, throttle: int, yaw: int) -> list[int]:
    """Create an armed frame with given channel values."""
    return [
        time_ms,
        roll,        # CH1 Roll
        pitch,       # CH2 Pitch
        throttle,    # CH3 Throttle
        yaw,         # CH4 Yaw
        CRSF_MAX,    # CH5 Arm (armed)
        CRSF_MIN,    # CH6
        CRSF_MIN,    # CH7
        CRSF_MIN,    # CH8
        CRSF_MID,    # CH9
        CRSF_MID,    # CH10
        CRSF_MID,    # CH11
        CRSF_MID,    # CH12
        CRSF_MID,    # CH13
        CRSF_MID,    # CH14
        CRSF_MID,    # CH15
        CRSF_MID,    # CH16
    ]


def convert(input_path: Path, output_path: Path) -> None:
    """Main conversion pipeline."""
    print(f"Reading blackbox CSV: {input_path}")
    raw_frames = parse_blackbox_csv(input_path)
    print(f"  Parsed {len(raw_frames)} raw frames")

    t_start = raw_frames[0]["time_us"]
    t_end = raw_frames[-1]["time_us"]
    duration_s = (t_end - t_start) / 1_000_000.0
    src_rate = len(raw_frames) / duration_s if duration_s > 0 else 0
    print(f"  Duration: {duration_s:.2f}s, source rate: {src_rate:.0f}Hz")

    print(f"Downsampling to {1000 // OUTPUT_INTERVAL_MS}Hz...")
    sampled = downsample(raw_frames, OUTPUT_INTERVAL_MS)
    print(f"  {len(sampled)} frames after downsampling")

    # Build output rows
    rows = []

    # Pre-flight disarm padding (3 seconds)
    disarm_frames = DISARM_PAD_MS // OUTPUT_INTERVAL_MS
    for i in range(disarm_frames):
        rows.append(make_disarm_frame(i * OUTPUT_INTERVAL_MS))

    time_offset = DISARM_PAD_MS

    # Flight data (armed)
    for frame in sampled:
        roll = rc_command_to_crsf(frame["roll"])
        pitch = rc_command_to_crsf(frame["pitch"])
        yaw = rc_command_to_crsf(frame["yaw"])
        throttle = throttle_to_crsf(frame["throttle"])
        t = time_offset + frame["time_ms"]
        rows.append(make_arm_frame(t, roll, pitch, throttle, yaw))

    # Post-flight disarm padding (3 seconds)
    last_time = rows[-1][0]
    for i in range(1, disarm_frames + 1):
        rows.append(make_disarm_frame(last_time + i * OUTPUT_INTERVAL_MS))

    # Write output CSV
    header = ["timestamp_ms"] + [f"ch{i}" for i in range(1, 17)]

    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)

    total_duration_s = rows[-1][0] / 1000.0
    print(f"Written {len(rows)} frames to {output_path}")
    print(f"  Total duration: {total_duration_s:.2f}s "
          f"(disarm {DISARM_PAD_MS / 1000:.0f}s + flight {duration_s:.2f}s + disarm {DISARM_PAD_MS / 1000:.0f}s)")


def main():
    parser = argparse.ArgumentParser(
        description="Convert Betaflight Blackbox CSV to ExpressLRS Sender CSV"
    )
    parser.add_argument("input", type=Path, help="Input blackbox CSV file")
    parser.add_argument(
        "-o", "--output", type=Path, default=None,
        help="Output CSV file (default: same name with _elrs suffix)"
    )
    args = parser.parse_args()

    if not args.input.exists():
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    if args.output is None:
        args.output = args.input.with_stem(args.input.stem + "_elrs").with_suffix(".csv")
        # If input is in data/blackbox/, output to data/
        if "blackbox" in str(args.input.parent):
            args.output = args.input.parent.parent / args.output.name

    convert(args.input, args.output)


if __name__ == "__main__":
    main()
