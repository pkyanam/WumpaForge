#!/usr/bin/env python3
"""Run the shared Mac/Android actual-source kernel dispatch regression."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).resolve().parents[3] / 'tools/test_kernel_dispatch_tls.py'),
                  run_name='__main__')
