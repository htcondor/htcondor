universe = standard
executable = ring
output = job_ligo_x86-64-chkpttst.out
error = job_ligo_x86-64-chkpttst.err
log = job_ligo_x86-64-chkpttst.log
arguments = --channel-name L1:LSC-DARM_ERR --bank-max-quality 20.0 --gps-end-time 793422376 --dynamic-range-factor 1.0e+20 --bank-max-frequency 2000 --calibration-cache data/l1_calibration_cache.txt --maximize-duration 1 --segment-duration 256 --cutoff-frequency 45 --gps-start-time 793420200 --highpass-frequency 40 --debug-level 1 --sample-rate 4096 --bank-min-quality 2.0 --bank-template-phase 0 --bank-min-frequency 50 --bank-max-mismatch 0.03 --frame-cache data/l1_frame_cache.txt --threshold 6.0 --verbose --bank-file L1-RINGBANK-793420200-2176.xml --block-duration 2176 --only-segment-numbers 1
Notification = Never

queue
