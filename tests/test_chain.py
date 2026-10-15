#!/usr/bin/env python3
"""Test réel des trois exécutables, démarrés dans l'ordre Ext/Intf_Ext/Interner."""
from contextlib import ExitStack
from pathlib import Path
import json
import subprocess
import time
import uuid

from test_ext_intf import QueuePeer, REQUEST, RESPONSE


def main():
    root = Path(__file__).resolve().parents[1]
    endpoint = Path('/tmp/lxc_demo.sock')
    queue_paths = [Path('/dev/mqueue') / name.decode().lstrip('/') for name in (REQUEST, RESPONSE)]
    if endpoint.exists() or endpoint.is_socket() or any(path.exists() for path in queue_paths):
        raise RuntimeError('Un endpoint existe déjà : aucun processus lancé')
    # Réutilisé seulement pour supprimer les noms après arrêt, sans ouvrir,
    # lire ou écrire les queues : Interner est ici le véritable correspondant.
    cleanup = QueuePeer()
    log_dir = root / 'logs' / ('test-chain-' + uuid.uuid4().hex[:12])
    log_dir.mkdir(parents=True)
    processes = {}
    events = []
    ready = False
    error = None
    snapshots = {}
    started = time.monotonic()

    def check_alive():
        for name, process in processes.items():
            if process.poll() is not None:
                raise RuntimeError(f'{name} arrêté prématurément : {process.returncode}')

    def wait_until(predicate, description):
        deadline = time.monotonic() + 3
        while True:
            check_alive()
            if predicate():
                return
            if time.monotonic() >= deadline:
                raise TimeoutError(description)
            time.sleep(0.01)

    def read_log(name):
        return (log_dir / (name + '.log')).read_text()

    expected = {
        'ext': ('[Ext]', ('TX: COUCOU', 'RX: POUET')),
        'intf_ext': ('[Intf_Ext]', ('Socket RX: COUCOU', 'Queue TX: COUCOU', 'Queue RX: POUET', 'Socket TX: POUET')),
        'interner': ('[Interner]', ('Queue RX: COUCOU', 'Queue TX: POUET')),
    }

    with ExitStack() as stack:
        outputs = {name: stack.enter_context((log_dir / (name + '.log')).open('w')) for name in expected}
        def launch(name):
            processes[name] = subprocess.Popen([str(root / name)], cwd=root, stdout=outputs[name], stderr=subprocess.STDOUT)

        try:
            launch('ext')
            wait_until(endpoint.is_socket, 'Socket Ext indisponible')
            ready = True
            events.append('Ext : socket disponible')
            launch('intf_ext')
            wait_until(lambda: all(path.exists() for path in queue_paths)
                       and '[Intf_Ext] Message queues ready:' in read_log('intf_ext'),
                       'Files Intf_Ext indisponibles')
            events.append('Intf_Ext : files disponibles')
            launch('interner')
            events.append('Interner : lancé après les files')

            def exchanges_completed():
                snapshots.update({name: read_log(name) for name in expected})
                return all(snapshots[name].count(prefix + ' ' + messages[-1] + '\n') >= 3
                           for name, (prefix, messages) in expected.items())
            wait_until(exchanges_completed, 'Trois échanges complets non observés')
            for name, (prefix, messages) in expected.items():
                # Ignorer uniquement une ligne en cours d'écriture.
                text = snapshots[name]
                complete_lines = text[:text.rfind('\n') + 1].splitlines()
                lines = [line for line in complete_lines if any(line.startswith(prefix + ' ' + message.split(':')[0] + ':') for message in messages)]
                for index, line in enumerate(lines):
                    if line != prefix + ' ' + messages[index % len(messages)]:
                        raise ValueError(f'{name} : échange incorrect : {line}')
        except Exception as exc:
            error = str(exc)
        finally:
            for process in processes.values():
                if process.poll() is None:
                    process.terminate()
            for process in processes.values():
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=2)
            if '[Intf_Ext] Message queues ready:' in read_log('intf_ext'):
                cleanup.unlink_owned_names()
            if ready and endpoint.exists():
                endpoint.unlink()

    resources_removed = not endpoint.exists() and all(not path.exists() for path in queue_paths)
    report = {
        'success': error is None and resources_removed,
        'error': error or (None if resources_removed else 'Endpoints non nettoyés'),
        'startup_order': events,
        'elapsed_seconds': round(time.monotonic() - started, 3),
        'completed_exchanges': read_log('ext').count('[Ext] RX: POUET\n'),
        'log_dir': str(log_dir), 'resources_removed': resources_removed,
        'processes': {name: {'pid': process.pid, 'exit_status': process.returncode, 'stopped': process.poll() is not None} for name, process in processes.items()},
    }
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report['success'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
