#!/usr/bin/env python3
"""Client de test pour Ext, à lancer manuellement sur la Pi.

Ext doit déjà être lancé et attendre sur son socket Unix. Ce script ne
compile rien et ne démarre aucun processus. Il utilise le protocole oalIPC :
longueur sur 4 octets en ordre réseau, puis contenu sans NUL ni retour ligne.
"""

import argparse
import select
import socket
import struct
import sys


MAX_MESSAGE_SIZE = 4096


def receive_exact(peer, size):
    data = bytearray()
    while len(data) < size:
        chunk = peer.recv(size - len(data))
        if not chunk:
            raise EOFError("Ext a fermé la connexion avant la fin du message")
        data.extend(chunk)
    return bytes(data)


def receive_message(peer):
    size = struct.unpack("!I", receive_exact(peer, 4))[0]
    if not 1 <= size <= MAX_MESSAGE_SIZE:
        raise ValueError(f"Longueur oalIPC invalide : {size}")
    return receive_exact(peer, size)


def expect_coucou(peer):
    message = receive_message(peer)
    print(f"[Test] RX: {message!r}", flush=True)
    if message != b"COUCOU":
        raise ValueError(f"Message attendu : COUCOU ; reçu : {message!r}")


def run_test(endpoint, exchanges, timeout):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
        peer.settimeout(timeout)
        peer.connect(endpoint)
        expect_coucou(peer)

        for index in range(exchanges):
            frame = struct.pack("!I", 5) + b"POUET"

            # Envoyer une réponse incomplète : Ext doit rester en attente.
            peer.sendall(frame[:6])
            readable, _, _ = select.select([peer], [], [], 0.05)
            if readable:
                raise ValueError("Ext a envoyé des données ou fermé le socket avant la réponse complète")

            peer.sendall(frame[6:])
            print(f"[Test] TX: POUET ({index + 1}/{exchanges})", flush=True)

            # Le COUCOU suivant confirme qu'Ext a accepté la réponse et
            # recommence. Il sert aussi de requête à l'itération suivante.
            expect_coucou(peer)

        # Une nouvelle requête a été reçue. Ne pas fermer brutalement avant
        # sa lecture : signaler maintenant EOF à Ext pendant son attente.
        peer.shutdown(socket.SHUT_WR)
        if peer.recv(1) != b"":
            raise ValueError("Ext a envoyé des données après la fin des échanges")

    print(f"PASS : {exchanges} échanges COUCOU/POUET, réponse fragmentée et fermeture", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--socket", default="/tmp/lxc_demo.sock", help="Chemin du socket d'Ext")
    parser.add_argument("--exchanges", type=int, default=3, help="Nombre d'échanges à vérifier")
    parser.add_argument("--timeout", type=float, default=3.0, help="Délai maximal d'une opération socket, en secondes")
    args = parser.parse_args()
    if args.exchanges < 1 or not 0 < args.timeout < float("inf"):
        parser.error("--exchanges et --timeout doivent être strictement positifs et finis")

    try:
        run_test(args.socket, args.exchanges, args.timeout)
    except (OSError, EOFError, ValueError) as error:
        print(f"FAIL : {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
