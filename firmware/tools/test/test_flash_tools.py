#!/usr/bin/env python3
"""Unit tests for the host side flash tooling.

Exercises check_flash_fit.py, the validator that decides whether the built
app binary fits its target partition with the required margin.

Run with:  python3 firmware/tools/test/test_flash_tools.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import check_flash_fit

# Minimal table shaped like the production one: an nvs partition at a fixed
# offset plus a 3700K app partition with no offset (auto layout).
PARTITION_CSV = (
    'nvs, data, nvs, 0x9000, 24K,\n'
    'x, app, ota_0, , 3700K,\n'
)


def write(path, data):
    if isinstance(data, str):
        data = data.encode()
    with open(path, 'wb') as handle:
        handle.write(data)
    return path

class TestFlashFit(unittest.TestCase):

    def setUp(self):
        import tempfile
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = self._tmp.name

    def tearDown(self):
        self._tmp.cleanup()

    def test_parse_explicit_and_implicit_offsets(self):
        csv = write(os.path.join(self.tmp, 'p.csv'), PARTITION_CSV.encode())
        parts = {p[0]: p[3] for p in check_flash_fit.parse_partition_table(csv)}
        self.assertEqual(0x9000, parts['nvs'])
        # Blank offset means ESP-IDF auto layout; the parser must not invent one.
        csv2 = write(os.path.join(self.tmp, 'p2.csv'),
                     'nvs, data, nvs, , 24K,\n')
        parts2 = {p[0]: p[3] for p in check_flash_fit.parse_partition_table(csv2)}
        self.assertEqual(0, parts2['nvs'])

    def test_smallest_app_partition_is_used(self):
        csv = write(os.path.join(self.tmp, 'p.csv'), PARTITION_CSV.encode())
        size = check_flash_fit.get_app_partition_size(
            check_flash_fit.parse_partition_table(csv), 'x')
        self.assertEqual(3700 * 1024 // 1024, size)

    def test_binaries_with_size_suffixes(self):
        csv = write(os.path.join(self.tmp, 'p.csv'),
                    'factory, app, factory, , 3M,\n')
        parts = check_flash_fit.parse_partition_table(csv)
        self.assertEqual(3 * 1024 * 1024, parts[0][4])

    def test_cli_rejects_tight_fit(self):
        csv = write(os.path.join(self.tmp, 'p.csv'),
                    'factory, app, factory, , 100K,\n')
        binary = write(os.path.join(self.tmp, 'a.bin'), b'\xff' * 99 * 1024)
        argv = sys.argv
        sys.argv = ['check_flash_fit.py', '--partition-table', csv,
                    '--app-bin', binary, '--min-free-percent', '5']
        try:
            self.assertEqual(1, check_flash_fit.main())
        finally:
            sys.argv = argv

    def test_cli_accepts_adequate_room(self):
        csv = write(os.path.join(self.tmp, 'p.csv'),
                    'factory, app, factory, , 100K,\n')
        binary = write(os.path.join(self.tmp, 'a.bin'), b'\xff' * 50 * 1024)
        argv = sys.argv
        sys.argv = ['check_flash_fit.py', '--partition-table', csv,
                    '--app-bin', binary]
        try:
            self.assertEqual(0, check_flash_fit.main())
        finally:
            sys.argv = argv


if __name__ == '__main__':
    unittest.main(verbosity=2)