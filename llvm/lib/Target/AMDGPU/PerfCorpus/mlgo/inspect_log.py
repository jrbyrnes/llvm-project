#!/usr/bin/env python3
"""
Inspect MLGO training log files.

Reads TFRecord log files produced by the MLCoExec scheduler and displays
the features, decisions, and rewards for inspection.

Usage:
    python3 inspect_log.py logs/kernel.tfrecord
    python3 inspect_log.py logs/kernel.tfrecord --limit 10
    python3 inspect_log.py logs/kernel.tfrecord --features
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path
from typing import List, Dict, Any, Optional


def read_tfrecord_raw(path: Path) -> List[bytes]:
    """Read raw records from a TFRecord file.

    TFRecord format:
    - uint64 length
    - uint32 masked_crc32_of_length
    - byte   data[length]
    - uint32 masked_crc32_of_data
    """
    records = []
    with open(path, "rb") as f:
        while True:
            # Read length
            length_bytes = f.read(8)
            if len(length_bytes) < 8:
                break
            length = struct.unpack("<Q", length_bytes)[0]

            # Skip CRC of length
            f.read(4)

            # Read data
            data = f.read(length)
            if len(data) < length:
                break

            # Skip CRC of data
            f.read(4)

            records.append(data)

    return records


def try_parse_protobuf(data: bytes) -> Optional[Dict[str, Any]]:
    """Try to parse data as a protobuf Example.

    This is a basic parser - for full support, use tensorflow.
    """
    try:
        # Try importing tensorflow for proper parsing
        import tensorflow as tf
        example = tf.train.Example()
        example.ParseFromString(data)

        features = {}
        for key, feature in example.features.feature.items():
            if feature.HasField("float_list"):
                features[key] = list(feature.float_list.value)
            elif feature.HasField("int64_list"):
                features[key] = list(feature.int64_list.value)
            elif feature.HasField("bytes_list"):
                features[key] = list(feature.bytes_list.value)

        return features
    except ImportError:
        return None
    except Exception:
        return None


def inspect_log(path: Path, limit: Optional[int] = None, show_features: bool = False):
    """Inspect a training log file."""
    if not path.exists():
        print(f"Error: File not found: {path}", file=sys.stderr)
        return 1

    print(f"Inspecting: {path}")
    print(f"File size: {path.stat().st_size:,} bytes")
    print()

    # Read raw records
    records = read_tfrecord_raw(path)
    print(f"Total records: {len(records)}")

    if limit:
        records = records[:limit]
        print(f"Showing first {limit} records")
    print()

    # Try to parse records
    for i, data in enumerate(records):
        print(f"--- Record {i} ({len(data)} bytes) ---")

        parsed = try_parse_protobuf(data)
        if parsed:
            if show_features:
                for key, value in sorted(parsed.items()):
                    if isinstance(value, list) and len(value) > 10:
                        print(f"  {key}: [{value[0]}, {value[1]}, ..., {value[-1]}] (len={len(value)})")
                    else:
                        print(f"  {key}: {value}")
            else:
                # Summary view
                keys = sorted(parsed.keys())
                print(f"  Features: {', '.join(keys[:10])}")
                if len(keys) > 10:
                    print(f"            ... and {len(keys) - 10} more")

                # Show reward if present
                if "reward" in parsed:
                    print(f"  Reward: {parsed['reward']}")
                if "index_to_sched" in parsed:
                    print(f"  Decision: {parsed['index_to_sched']}")
        else:
            # Raw hex dump
            print(f"  (Could not parse as protobuf)")
            print(f"  Raw: {data[:100].hex()}...")

        print()

    if limit and len(records) < limit:
        print(f"(Showing all {len(records)} records)")

    return 0


def main():
    parser = argparse.ArgumentParser(description="Inspect MLGO training logs")
    parser.add_argument("log_file", type=Path, help="Path to .tfrecord log file")
    parser.add_argument("--limit", "-n", type=int, default=None,
                        help="Limit number of records to show")
    parser.add_argument("--features", "-f", action="store_true",
                        help="Show all feature values")
    args = parser.parse_args()

    return inspect_log(args.log_file, args.limit, args.features)


if __name__ == "__main__":
    sys.exit(main())
