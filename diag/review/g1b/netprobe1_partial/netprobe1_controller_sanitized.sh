#!/bin/bash
# NETPROBE1 per G1B_NETPROBE1_GATE.md @ 2c9bc2e (host-only ICMP). Shell commands only; no repository tool.
DEV=<DEVICE_IP>; DEVMAC=<DEVICE_MAC>
D=/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr203/diag/netprobe1_20261015
P1=$D/phase1_bursts.log; P2=$D/phase2_dense.log; EV=$D/events.log; STOPF=$D/STOPPED
T0=$1                       # t_start (epoch seconds, integer)
ev(){ echo "$(date +%s.%N) $*" >> $EV; }
nb(){ ip neigh show $DEV 2>&1 | head -1; }
stop(){ ev "STOP/HOLD $*"; echo "$*" > $STOPF; exit 3; }
sleep_until(){ now=$(date +%s.%N); d=$(python3 -c "print(max(0,$1-$now))"); sleep $d; }
ev "t_start=$T0 cmd_phase1='ping -4 -n -D -c 3 -i 0.2 -W 2 DEV' cmd_phase2='timeout -k 5 1830 ping -4 -n -D -i 0.2 -W 2 -w 1800 DEV'"
ev "ntp=$(timedatectl show -p NTPSynchronized)"; ev "route: $(ip route get $DEV | head -1)"; ev "neigh_initial: $(nb)"
zero=0
for i in $(seq 0 19); do
  plan=$((T0+180*i)); sleep_until $plan
  b="$(nb)"; start=$(date +%s.%N)
  echo "### burst $i planned=$plan start=$start neigh_before='$b'" >> $P1
  out=$(ping -4 -n -D -c 3 -i 0.2 -W 2 $DEV 2>&1); rc=$?
  end=$(date +%s.%N); a="$(nb)"
  echo "$out" >> $P1
  echo "### burst $i end=$end exit=$rc neigh_after='$a'" >> $P1
  ev "burst $i planned=$plan start=$start end=$end exit=$rc neigh_before='$b' neigh_after='$a'"
  n=$(echo "$out" | grep -c 'bytes from')
  if echo "$out" | grep 'bytes from' | grep -v "bytes from $DEV:" | grep -q .; then stop "burst $i reply source != device IP"; fi
  if echo "$out" | grep 'bytes from' | grep -v "ttl=64 " | grep -q .; then stop "burst $i reply TTL != 64"; fi
  for s in "$b" "$a"; do if echo "$s" | grep -q lladdr && ! echo "$s" | grep -qi "lladdr $DEVMAC"; then stop "burst $i neighbor MAC differs"; fi; done
  if [ "$n" -eq 0 ]; then zero=$((zero+1)); else zero=0; fi
  if [ "$zero" -ge 3 ]; then stop "VERBINDUNGSVERLUST (3 consecutive bursts without reply), cause UNKNOWN"; fi
done
sleep_until $((T0+3600))
ev "phase2 start; neigh='$(nb)'"
( timeout -k 5 1830 ping -4 -n -D -i 0.2 -W 2 -w 1800 $DEV > $P2 2>&1; echo "exit=$?" > $D/phase2_exit.txt; date +%s.%N >> $D/phase2_exit.txt ) &
PP=$!
last=$(date +%s)
while kill -0 $PP 2>/dev/null; do
  sleep 5
  l=$(grep 'bytes from' $P2 2>/dev/null | tail -1 | sed -E 's/^\[([0-9.]+)\].*/\1/')
  if [ -n "$l" ]; then last=${l%.*}; fi
  if [ $(( $(date +%s) - last )) -ge 60 ]; then pkill -f "ping -4 -n -D -i 0.2 -W 2 -w 1800" ; ev "VERBINDUNGSVERLUST (60 s without reply in phase 2), cause UNKNOWN"; echo "loss" > $STOPF; break; fi
  if grep 'bytes from' $P2 | grep -v "bytes from $DEV:" | grep -q .; then pkill -f "ping -4 -n -D -i 0.2 -W 2 -w 1800"; stop "phase2 reply source != device"; fi
  if grep 'bytes from' $P2 | grep -v "ttl=64 " | grep -q .; then pkill -f "ping -4 -n -D -i 0.2 -W 2 -w 1800"; stop "phase2 reply TTL != 64"; fi
done
wait $PP 2>/dev/null
ev "phase2 end; neigh='$(nb)'"; ev "link: $(ip -s link show enp2s0 | tr '\n' ' ' | cut -c1-400)"; ev "DONE"
