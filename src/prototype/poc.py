import socket
import time
import threading
from dataclasses import dataclass
import subprocess

HEADER_MAGIC = b'\x90\x60'
HEADER_SIZE = 22

prop_buffer = b''

fu_buffer = b""

# =======================
# Configuration
# =======================

@dataclass
class Config:
    cam_ip: str = "192.179.8.1"
    cam_port: int = 6320

    udp_target_port: int = 8989
    udp_local_port: int = 14766

    dump_file: str = "udp_dump.bin"
    recv_timeout: float = 5.0


CFG = Config()


# =======================
# Helpers
# =======================

def hexdump(data: bytes) -> str:
    return " ".join(f"{b:02x}" for b in data)


def send_tcp_packet(sock: socket.socket, name: str, packet: bytes):
    print(f"[TCP] {name}: {hexdump(packet)}")
    sock.sendall(packet)

    try:
        resp = sock.recv(1024)
        if resp:
            print(f"[TCP] {name} response: {resp.hex()}")
    except socket.timeout:
        pass


# =======================
# Protocol
# =======================

LOGIN_PACKET = bytes.fromhex(
    "7e 0f 10 11 00 06 68 6f 6e 67 62 6f "
    "08 32 30 31 35 30 31 30 31 10 0d"
)

START_PACKET = bytes.fromhex(
    "7e 0f 11 04 00 00 00 00 31 0d"
)

CONT_PACKET = bytes.fromhex(
    "7e 0f 17 05 00 69 e2 64 0a 71 0d"
)

FORCEIFRAME_PACKET = bytes.fromhex(
    "7e 0f 72 04 00 00 00 00  d0 0d"
)

def build_udp_setup_packet(local_ip: str, local_port: int) -> bytes:
    ip_bytes = bytes(int(x) for x in local_ip.split("."))
    port_byte = bytes([local_port & 0xFF])

    payload = b"\x00" + ip_bytes + port_byte
    checksum = sum(payload) & 0xFF

    return b"\x7e\x0f\x17\x05" + payload + bytes([checksum]) + b"\x0d"

def keepalive_loop(sock: socket.socket, interval=0.2):
    counter = 0
    while True:
        pkt = build_cont_packet(counter)
        sock.sendall(pkt)
        counter = (counter + 1) & 0xFF
        time.sleep(1.0)

def build_heartbeat(counter: int) -> bytes:
    session = b"\x69\xe3\xda"

    counter &= 0xFF
    counter_pair = (0x104 - counter) & 0xFF

    payload = b"\x00" + session + bytes([counter, counter_pair])

    return b"\x7e\x0f\x17\x05" + payload + b"\x0d"
    
def send_one_init_hb(sock: socket.socket):
    pkt = build_heartbeat(0xC1)
    print("[init HB] ", pkt)

    try:
        sock.sendall(pkt)
        print(f"[HB] sent")
    except Exception as e:
        print("[HB] stopped:", e)
    
def heartbeat_loop(sock: socket.socket, start=0xC2, interval=1.0):
    counter = start

    while True:
        pkt = build_heartbeat(counter)
        print("[HB] ", pkt)

        try:
            sock.sendall(pkt)
            print(f"[HB] {counter:02x}")
        except Exception as e:
            print("[HB] stopped:", e)
            break

        counter = (counter + 1) & 0xFF
        time.sleep(interval)


def build_cont_packet(counter):
    payload = bytes.fromhex("00 69 e2 64") + bytes([counter & 0xFF])
    chk = sum(payload) & 0xFF
    return b"\x7e\x0f\x17\x05" + payload + bytes([chk]) + b"\x0d"

# =======================
# UDP Receiver
# =======================

def start_udp_receiver(cfg: Config):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("", cfg.udp_local_port))
    sock.settimeout(cfg.recv_timeout)

    print(f"[UDP] Listening on {cfg.udp_local_port}")

    def worker():
        with open(cfg.dump_file, "wb") as f:
            try:
                while True:
                    data, addr = sock.recvfrom(4096)
                    #print(f"[UDP] {len(data)} bytes from {addr}")
                    f.write(data)
            except socket.timeout:
                print("[UDP] Timeout reached")

        sock.close()
        print(f"[UDP] Dump written to {cfg.dump_file}")

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()

    # send trigger immediately after bind
    sock.sendto(b"\x01", (cfg.cam_ip, cfg.udp_target_port))
    print(f"[UDP] Trigger sent from port {cfg.udp_local_port}")

    return thread

def strip_rtp(packet: bytes) -> bytes:
    if len(packet) > 12:
        return packet[12:]
    return b""

def extract_h264(packet: bytes) -> bytes:
    # find H264 start code
    for i in range(len(packet) - 4):
        if packet[i:i+3] == b"\x00\x00\x01" or packet[i:i+4] == b"\x00\x00\x00\x01":
            return packet[i:]
    return b""
    
sps = None
pps = None
started = False

def extract_proprietary_packets(data: bytes) -> list[bytes]:
    """
    Extracts H264 payloads by removing proprietary headers.
    Returns a list of payload chunks.
    """
    global prop_buffer
    prop_buffer += data

    out = []

    while True:
        start = prop_buffer.find(HEADER_MAGIC)
        if start == -1:
            prop_buffer = prop_buffer[-2:]
            break

        if len(prop_buffer) < start + HEADER_SIZE:
            break

        next_start = prop_buffer.find(HEADER_MAGIC, start + 2)

        if next_start == -1:
            break

        payload = prop_buffer[start + HEADER_SIZE:next_start]
        out.append(payload)

        prop_buffer = prop_buffer[next_start:]

    return out

def extract_and_sync(packet: bytes):
    global sps, pps, started

    out = []

    i = 0
    while i < len(packet) - 4:
        if packet[i:i+3] == b"\x00\x00\x01":
            start = i
            i += 3

            # find next start code
            next_start = packet.find(b"\x00\x00\x01", i)
            if next_start == -1:
                nal = packet[start:]
                i = len(packet)
            else:
                nal = packet[start:next_start]
                i = next_start

            nal_type = nal[3] & 0x1F if len(nal) > 4 else -1

            if nal_type == 7:   # SPS
                sps = nal
                print("[H264] SPS found")

            elif nal_type == 8: # PPS
                pps = nal
                print("[H264] PPS found")

            elif nal_type in (5, 1):  # IDR or slice
                if sps and pps:
                    if not started:
                        print("[H264] Starting stream (have SPS/PPS)")
                        out.append(sps)
                        out.append(pps)
                        started = True

                    out.append(nal)

    return b"".join(out)
    
    
def parse_rtp_h264(payload: bytes):
    global fu_buffer

    if not payload:
        return b""

    nal_type = payload[0] & 0x1F

    # Single NAL
    if nal_type < 24:
        return b"\x00\x00\x00\x01" + payload

    # FU-A
    elif nal_type == 28:
        fu_indicator = payload[0]
        fu_header = payload[1]

        start = fu_header & 0x80
        end   = fu_header & 0x40
        nal_type = fu_header & 0x1F

        if start:
            fu_buffer = b"\x00\x00\x00\x01" + bytes([
                (fu_indicator & 0xE0) | nal_type
            ]) + payload[2:]
            return b""

        else:
            fu_buffer += payload[2:]

            if end:
                nal = fu_buffer
                fu_buffer = b""
                return nal

    return b""

    
def start_udp_to_mpv(cfg: Config):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("", cfg.udp_local_port))
    sock.settimeout(cfg.recv_timeout)

    print(f"[UDP] Listening on {cfg.udp_local_port}")

    # start mpv reading from stdin
    mpv = subprocess.Popen(
           [
        "mpv",
        "--no-cache",
        "--untimed",
        "--demuxer-lavf-format=h264",
        "-"
    ],
        stdin=subprocess.PIPE
    )

    def worker():
        try:
            while True:
                data, addr = sock.recvfrom(4096)
                #print(f"[UDP] {len(data)} bytes")

                # write directly into mpv
                chunks = extract_proprietary_packets(data)
                
                for chunk in chunks:
                    if chunk and mpv.stdin:
                        mpv.stdin.write(chunk)
                        mpv.stdin.flush()

        except socket.timeout:
            print("[UDP] Timeout")

        finally:
            sock.close()
            if mpv.stdin:
                mpv.stdin.close()
            mpv.wait()

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()

    # trigger
    sock.sendto(b"\x01", (cfg.cam_ip, cfg.udp_target_port))
    print("[UDP] Trigger sent")

    return thread

# =======================
# Main flow
# =======================

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(1)
        s.connect((CFG.cam_ip, CFG.cam_port))

        # LOGIN
        send_tcp_packet(s, "LOGIN", LOGIN_PACKET)
        time.sleep(0.2)
        
        send_tcp_packet(s, "Force IFrame", FORCEIFRAME_PACKET)
        time.sleep(0.2)

        # start UDP + send trigger
        udp_thread = start_udp_to_mpv(CFG)
        time.sleep(0.2)

        send_one_init_hb(s)
        time.sleep(0.2)

        send_tcp_packet(s, "START_STREAM", START_PACKET)
        time.sleep(0.2)
        
        hb_thread = threading.Thread(
            target=heartbeat_loop,
            args=(s,),
            daemon=True
        )
        hb_thread.start()

        print("[MAIN] Waiting for UDP stream...")
        time.sleep(200)


if __name__ == "__main__":
    main()
    
