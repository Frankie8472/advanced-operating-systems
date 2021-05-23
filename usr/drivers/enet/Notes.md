# Notes 'n Commands 'n Stuff

## Notes
somehow, ping only works from the debian-laptop

## Commands
- arping
  `sudo arping -I enp2s0 10.0.2.1 -S 127.0.0.1`
- regular ping
  `ping 10.0.2.1 -I enp2s0`
- set route to board
  `sudo ip route add 10.0.2.1 dev enp2s0`
