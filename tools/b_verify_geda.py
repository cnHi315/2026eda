#!/usr/bin/env python3
"""B: verify a demo export using original gEDA PCB (not pcb-rnd).

Requires a running graphical DISPLAY. Runs PCB's own LoadFrom/AddRats/SaveTo
actions and checks the saved result, rather than trusting a zero exit code.
Example:
  python3 tools/b_verify_geda.py --pcb-bin /path/to/pcb-gtk \
      --pcb-data /path/to/share/pcb --layout samples/demo.pcb \
      --netlist samples/demo.net --output build-week9/evidence/geda-run
"""
import argparse
import collections
from decimal import Decimal
import json
from pathlib import Path
import re
import shlex
import subprocess
import time


def coordinate(token):
    match = re.fullmatch(r"(-?[\d.]+)(mil|mm|um)?", token)
    if not match:
        raise ValueError(f"Unsupported PCB coordinate: {token}")
    factors = {None: Decimal(1), "mil": Decimal(100),
               "mm": Decimal(100000) / Decimal('25.4'),
               "um": Decimal(100) / Decimal('25.4')}
    return int((Decimal(match[1]) * factors[match[2]]).to_integral_value())


def inspect(saved, expected):
    text = saved.read_text()
    networks = {}
    for name, body in re.findall(r'\bNet\("([^"]+)"[^\n]*\)\s*\((.*?)\n\s*\)', text, re.S):
        if name in networks:
            raise ValueError(f"Duplicate network: {name}")
        networks[name] = set(re.findall(r'Connect\("([^"]+)"\)', body))
    if networks != expected:
        raise ValueError(f"Imported network mismatch: {networks!r} != {expected!r}")
    pins, rats = {}, []
    ref, origin = None, (0, 0)
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('Element['):
            fields = shlex.split(line[len('Element['):-1])
            ref = fields[2]
            origin = coordinate(fields[4]), coordinate(fields[5])
        elif line.startswith('Pin['):
            fields = shlex.split(line[len('Pin['):-1])
            pins[f'{ref}-{fields[7]}'] = (origin[0] + coordinate(fields[0]),
                                          origin[1] + coordinate(fields[1]))
        elif line.startswith('Rat['):
            fields = shlex.split(line[len('Rat['):-1])
            rats.append(((coordinate(fields[0]), coordinate(fields[1])),
                         (coordinate(fields[3]), coordinate(fields[4]))))
    # The demo contains exactly two endpoints per net and no copper routing.
    if len(expected) != 3 or any(len(ends) != 2 for ends in expected.values()):
        raise ValueError('This acceptance check requires three two-endpoint demo nets')
    wanted = collections.Counter()
    for name, ends in expected.items():
        points = [pins[end] for end in ends]
        if points[0] == points[1]:
            raise ValueError(f"Overlapping pads for network {name}: {points}")
        wanted[frozenset(points)] += 1
    actual = collections.Counter(frozenset(pair) for pair in rats)
    if actual != wanted:
        raise ValueError(f"Rats mismatch: {rats!r}; expected one connection per network")
    return {'networks': {k: sorted(v) for k, v in networks.items()},
            'rat_count': len(rats), 'rats': rats, 'pins': pins}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pcb-bin', required=True)
    parser.add_argument('--pcb-data', type=Path)
    parser.add_argument('--layout', type=Path, default=Path('samples/demo.pcb'))
    parser.add_argument('--netlist', type=Path, default=Path('samples/demo.net'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    layout, netlist = args.layout.resolve(), args.netlist.resolve()
    result = {'layout': str(layout), 'netlist': str(netlist), 'passed': False}
    started = time.monotonic()
    try:
        expected = {}
        for line in netlist.read_text().splitlines():
            if line.strip():
                name, *ends = line.split()
                if name in expected:
                    raise ValueError(f'Duplicate input net: {name}')
                expected[name] = set(ends)
        saved = output / 'imported.pcb'
        # Quote paths for PCB's action parser, and reject embedded line breaks.
        def quoted(path):
            value = str(path)
            if any(char in value for char in '\r\n"\\'):
                raise ValueError('Unsupported action-script path')
            return f'"{value}"'
        script = output / 'import.actions'
        script.write_text(f'LoadFrom(Netlist,{quoted(netlist)})\n'
                          f'AddRats(AllRats)\n'
                          f'SaveTo(LayoutAs,{quoted(saved)})\nQuit(force)\n')
        binary = str(Path(args.pcb_bin).resolve()) if '/' in args.pcb_bin else args.pcb_bin
        common = [binary]
        if args.pcb_data:
            data = args.pcb_data.resolve()
            common += ['--pcb-menu', str(data/'gpcb-menu.res'),
                       '--default-font', str(data/'default_font'),
                       '--lib-path', str(data), '--lib-command-dir', str(data)]
        with (output/'version.log').open('w') as log:
            version = subprocess.run([binary, '--version'], stdout=log, stderr=subprocess.STDOUT, timeout=15)
        if version.returncode:
            raise ValueError(f'PCB version query failed: {version.returncode}')
        version_text = (output/'version.log').read_text()
        if not re.search(r'^PCB version ', version_text, re.M) or 'pcb-rnd' in version_text.lower():
            raise ValueError('Original gEDA PCB is required; pcb-rnd cannot substitute')
        command = common + ['--action-script', str(script), str(layout)]
        result['command'] = command
        with (output/'import.log').open('w') as log:
            completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=45)
        result['exit_code'] = completed.returncode
        if completed.returncode:
            raise ValueError(f'PCB failed: {completed.returncode}')
        result.update(inspect(saved, expected))
        result['passed'] = True
    except (ValueError, OSError, KeyError, subprocess.TimeoutExpired) as error:
        result['error'] = str(error)
    result['elapsed_seconds'] = round(time.monotonic()-started, 3)
    (output/'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
