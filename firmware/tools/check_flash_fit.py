#!/usr/bin/env python3
"""Flash size validation script for Minino firmware."""
import argparse
import csv
import struct
import sys
import os

def parse_partition_table(csv_path):
    """Parse ESP-IDF partition CSV and return list of (name, type, subtype, offset, size) tuples."""
    partitions = []
    with open(csv_path, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or (row[0].strip().startswith('#') if row else False):
                continue
            if len(row) < 5:
                continue
            name = row[0].strip()
            type_ = row[1].strip()
            subtype = row[2].strip()
            offset = row[3].strip()
            size = row[4].strip()
            if not name or not type_ or not subtype:
                continue
            try:
                offset = int(offset, 0) if offset.startswith('0x') else int(offset) if offset else 0
                if size.endswith('K') or size.endswith('k'):
                    size = int(size[:-1]) * 1024
                elif size.endswith('M') or size.endswith('m'):
                    size = int(size[:-1]) * 1024 * 1024
                else:
                    size = int(size, 0) if size.startswith('0x') else int(size)
                if size == 0:
                    continue
                partitions.append((name, type_, subtype, offset, size))
            except ValueError:
                continue
    return partitions

def get_app_partition_size(partitions, label):
    """Find the smallest app partition (type=app) and return its size in KiB."""
    app_parts = [p for p in partitions if p[1] == 'app']
    if not app_parts:
        # Fallback: look for factory partition (type=app, subtype=factory)
        app_parts = [p for p in partitions if p[1] == 'app' and p[2] == 'factory']
    if not app_parts:
        # Fallback: look for any app subtype
        app_parts = [p for p in partitions if p[1] == 'app']
    if not app_parts:
        raise ValueError(f"[{label}] No app/factory partition found in partition table")
    smallest = min(app_parts, key=lambda p: p[4])
    return smallest[4] // 1024

def get_app_binary_size(bin_path):
    """Return size of app binary in KiB."""
    return os.path.getsize(bin_path) // 1024

def main():
    parser = argparse.ArgumentParser(description='Validate app binary fits in partition with minimum free space')
    parser.add_argument('--partition-table', required=True, help='Path to partition CSV')
    parser.add_argument('--app-bin', required=True, help='Path to app binary')
    parser.add_argument('--min-free-percent', type=float, default=5.0, help='Minimum free space percentage (default: 5)')
    parser.add_argument('--label', default='app', help='Label for output')
    args = parser.parse_args()

    if not os.path.exists(args.partition_table):
        print(f"[{args.label}] ERROR: Partition table not found: {args.partition_table}")
        return 1
    if not os.path.exists(args.app_bin):
        print(f"[{args.label}] ERROR: App binary not found: {args.app_bin}")
        return 1

    partitions = parse_partition_table(args.partition_table)
    try:
        partition_size_kib = get_app_partition_size(partitions, args.label)
    except ValueError as e:
        print(f"[{args.label}] ERROR: {e}")
        return 1

    app_size_kib = get_app_binary_size(args.app_bin)
    free_kib = partition_size_kib - app_size_kib
    free_percent = (free_kib / partition_size_kib) * 100

    print(f"[{args.label}] partition size: {partition_size_kib:.1f} KiB")
    print(f"[{args.label}] image size: {app_size_kib:.1f} KiB")
    print(f"[{args.label}] free: {free_kib:+.1f} KiB ({free_percent:.2f}%)")

    if free_percent < args.min_free_percent:
        print(f"[{args.label}] FAIL: free space {free_percent:.2f}% below minimum {args.min_free_percent}%")
        return 1

    print(f"[{args.label}] OK")
    return 0

if __name__ == '__main__':
    sys.exit(main())