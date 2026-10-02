#!/usr/bin/env python3
"""Verify separately named acceptance image using the repository layout verifier."""
import hashlib
import json
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from verify_firmware import verify_flash_images, verify_firmware_layout

build = Path(sys.argv[1]).resolve()
config = json.loads((build / 'flasher_args.json').read_text())
assert config['flash_settings']['flash_size'] == '8MB'
assert config['app']['file'] == 'R3-Acceptance.bin'
merged = (build / 'R3-Acceptance-full.bin').read_bytes()
images = {name: int(offset, 16) for offset, name in config['flash_files'].items()}
verify_flash_images(merged, build, images)
verify_firmware_layout(merged, build, int(config['partition-table']['offset'], 16),
                       int(config['app']['offset'], 16), 'R3-Acceptance.bin')
names = list(images) + ['R3-Acceptance-full.bin', 'R3-Acceptance.elf', 'R3-Acceptance.map', 'sdkconfig']
manifest = {'project': 'R3-Acceptance', 'target': 'esp32c3', 'idf': '5.5.3',
            'device_tests': 'NOT RUN', 'artifacts': {name: {'bytes': (build / name).stat().st_size,
            'sha256': hashlib.sha256((build / name).read_bytes()).hexdigest()} for name in names}}
assert 'CONFIG_IDF_TARGET="esp32c3"' in (build / 'sdkconfig').read_text()
assert 'CONFIG_SPIRAM=y' not in (build / 'sdkconfig').read_text()
(build / 'r3-acceptance-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest, indent=2))
print('Separate R3 C3 acceptance merge/layout/artifact verification: PASS (NOT RUN)')
