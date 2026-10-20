"""Connects to a running RobotDog device (e.g. server_example) over the openDAQ native protocol,
reads some signals for a while and reports what arrived:

  * samples per second and the spacing of the domain (time) values, including gaps
    (and the reader events that gaps produce),
  * min / max / an example value per signal,
  * how far the timestamps lag behind this PC's clock (should stay small and flat;
    a lag that keeps growing means the device's time axis is drifting).

Requires the openDAQ Python package:  py -m pip install openDAQ

usage:  py tools\\check_stream.py [--seconds 10] [--url daq.nd://127.0.0.1] [--signals FL_force current ...]
"""
import argparse
import collections
import datetime as dt
import time

import opendaq as daq

DEFAULT_SIGNALS = ["FL_force", "current", "voltage", "motorFR1_temperature", "motorRL3_temperature"]


def parse_args():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--seconds", type=float, default=10, help="how long to read (default 10)")
    p.add_argument("--url", default="daq.nd://127.0.0.1", help="device connection string (default daq.nd://127.0.0.1)")
    p.add_argument("--signals", nargs="+", default=DEFAULT_SIGNALS, help="signal names to read")
    return p.parse_args()


def drain(reader, events):
    """Yields everything queued in the reader. A read stops at every event (e.g. a gap in the
    domain), and available_count only counts up to the next one, so keep reading past events."""
    while True:
        values, domain_values, status = reader.read_with_domain(100_000, 0, True)
        if len(values):
            yield values, domain_values
        if status.read_status == daq.ReadStatus.Event:
            packet = status.event_packet
            events[packet.event_id if packet is not None else "unknown"] += 1
            continue
        if len(values) == 0:
            return


def percentile(sorted_values, q):
    return sorted_values[min(len(sorted_values) - 1, int(q * len(sorted_values)))]


def main():
    args = parse_args()

    instance = daq.Instance()
    device = instance.add_device(args.url)
    signals = {s.name: s for s in device.signals_recursive}
    print(f"Connected to {args.url}: {len(signals)} signals")

    readers = {}
    for name in args.signals:
        if name not in signals:
            print(f"  signal '{name}' not found, skipped")
            continue
        readers[name] = daq.StreamReader(signals[name], value_type=daq.SampleType.Float64, domain_type=daq.SampleType.Int64)
    if not readers:
        print("Nothing to read.")
        return

    first = next(iter(readers))
    domain = signals[first].domain_signal.descriptor
    resolution = domain.tick_resolution
    tick_s = resolution.numerator / resolution.denominator
    origin = dt.datetime.fromisoformat(str(domain.origin).replace("Z", "+00:00"))
    print(f"Domain: resolution {resolution.numerator}/{resolution.denominator} s, origin {domain.origin}")
    print(f"Reading for {args.seconds:.0f} s ...")

    stats = {n: {"count": 0, "min": None, "max": None, "example": None, "steps": {}, "last": None} for n in readers}
    events = {n: collections.Counter() for n in readers}
    lags_ms = []  # (seconds since start, lag) for the first signal
    t_start = time.time()
    while time.time() - t_start < args.seconds:
        time.sleep(0.1)
        for name, reader in readers.items():
            got_data = False
            for values, domain_values in drain(reader, events[name]):
                got_data = True
                st = stats[name]
                st["count"] += len(values)
                if st["example"] is None:
                    st["example"] = float(values[len(values) // 2])
                lo, hi = float(min(values)), float(max(values))
                st["min"] = lo if st["min"] is None else min(st["min"], lo)
                st["max"] = hi if st["max"] is None else max(st["max"], hi)
                previous = st["last"]
                for d in domain_values:
                    d = int(d)
                    if previous is not None:
                        st["steps"][d - previous] = st["steps"].get(d - previous, 0) + 1
                    previous = d
                st["last"] = previous
            if got_data and name == first:
                previous = stats[name]["last"]
                newest = origin + dt.timedelta(seconds=previous * tick_s)
                lag = (dt.datetime.now(dt.timezone.utc) - newest).total_seconds() * 1000
                lags_ms.append((time.time() - t_start, lag))

    print(f"\n{'signal':22s} {'samples':>8s} {'per s':>7s} {'min':>9s} {'max':>9s} {'example':>10s}  domain steps [us x count]")
    for name, st in stats.items():
        steps = sorted(st["steps"].items(), key=lambda kv: -kv[1])
        shown = ", ".join(f"{step}x{count}" for step, count in steps[:4]) + (" ..." if len(steps) > 4 else "")
        fmt = lambda v: "-" if v is None else f"{v:.4g}"
        print(f"{name:22s} {st['count']:8d} {st['count'] / args.seconds:7.1f} {fmt(st['min']):>9s} {fmt(st['max']):>9s} "
              f"{fmt(st['example']):>10s}  {shown}")

    for name, counter in events.items():
        if counter:
            print(f"{name}: reader events " + ", ".join(f"{eid} x{n}" for eid, n in counter.most_common()))

    if lags_ms:
        # The first reads can contain data buffered while connecting; skip the first second.
        settled = [lag for t, lag in lags_ms if t > 1.0] or [lag for _, lag in lags_ms]
        first_half = settled[: len(settled) // 2] or settled
        second_half = settled[len(settled) // 2:] or settled
        ordered = sorted(settled)
        print(f"\nTimestamp lag behind this PC's clock ({first}), after the first second:")
        print(f"  median {percentile(ordered, 0.5):.1f} ms, min {ordered[0]:.1f} ms, max {ordered[-1]:.1f} ms")
        print(f"  first half avg {sum(first_half) / len(first_half):.1f} ms -> second half avg "
              f"{sum(second_half) / len(second_half):.1f} ms (a steady increase means the time axis drifts)")


if __name__ == "__main__":
    main()
